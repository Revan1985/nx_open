// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "routing_management_widget.h"
#include "ui_routing_management_widget.h"

#include <algorithm>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_discovery_manager.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/input_dialog.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/server_addresses_model.h>
#include <utils/common/event_processors.h>
using namespace nx::vms::client::core;
using namespace nx::vms::client::desktop;

namespace {

    class SortedServersProxyModel : public QSortFilterProxyModel {
    public:
        SortedServersProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override
        {
            Q_UNUSED(orientation)

            switch (role)
            {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
                if (section == 0)
                    return QnRoutingManagementWidget::tr("Server");
                break;

            default:
                break;
            }

            return QVariant();
        }

    protected:
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
            QString leftString = left.data(sortRole()).toString();
            QString rightString = right.data(sortRole()).toString();
            return nx::utils::naturalStringLess(leftString, rightString);
        }
    };

    static void getAddresses(const QnMediaServerResourcePtr &server, QSet<nx::Url> &autoUrls, QSet<nx::Url> &additionalUrls, QSet<nx::Url> &ignoredUrls) {
        for (const nx::network::SocketAddress &address: server->getNetAddrList())
        {
            autoUrls.insert(nx::network::url::Builder()
                .setScheme(nx::network::http::kSecureUrlSchemeName)
                .setEndpoint(address)
                .toUrl());
        }

        for (const nx::Url &url: server->getAdditionalUrls())
            additionalUrls.insert(url);

        for (const nx::Url &url: server->getIgnoredUrls())
            ignoredUrls.insert(url);
    }

} // namespace

class RoutingChange {
public:
    QHash<nx::Url, bool> addresses;
    QHash<nx::Url, bool> ignoredAddresses;

    void apply(QSet<nx::Url> &additionalUrls, QSet<nx::Url> &ignoredUrls) const {
        processSet(additionalUrls, addresses);
        processSet(ignoredUrls, ignoredAddresses);
    }

    void simplify(const QSet<nx::Url> &autoUrls, const QSet<nx::Url> &additionalUrls, const QSet<nx::Url> &ignoredUrls, int port) {
        for (auto it = addresses.begin(); it != addresses.end(); /* no inc */) {
            nx::Url url = it.key();
            nx::Url defaultUrl = url;
            defaultUrl.setPort(port);

            if (autoUrls.contains(url) || it.value() == (additionalUrls.contains(url) || additionalUrls.contains(defaultUrl)))
                it = addresses.erase(it);
            else
                ++it;
        }
        for (auto it = ignoredAddresses.begin(); it != ignoredAddresses.end(); /* no inc */) {
            nx::Url url = it.key();
            nx::Url defaultUrl = url;
            defaultUrl.setPort(port);

            if (it.value() == (ignoredUrls.contains(url) || ignoredUrls.contains(defaultUrl)))
                it = ignoredAddresses.erase(it);
            else
                ++it;
        }
    }

    bool isEmpty() const {
        return addresses.isEmpty() && ignoredAddresses.isEmpty();
    }

    static RoutingChange diff(const QSet<nx::Url> &additionalUrlsA, const QSet<nx::Url> &ignoredUrlsA,
                              const QSet<nx::Url> &additionalUrlsB, const QSet<nx::Url> &ignoredUrlsB)
    {
        RoutingChange change;

        QSet<nx::Url> removed = additionalUrlsA;
        QSet<nx::Url> added = additionalUrlsB;
        for (auto it = removed.begin(); it != removed.end(); /* no inc */) {
            nx::Url url = *it;
            nx::Url implicitUrl = url;
            implicitUrl.setPort(-1);

            if (added.contains(url) || added.contains(implicitUrl)) {
                added.remove(url);
                added.remove(implicitUrl);
                it = removed.erase(it);
            } else {
                ++it;
            }
        }

        for (const nx::Url &url: removed)
            change.addresses.insert(url, false);
        for (const nx::Url &url: added)
            change.addresses.insert(url, true);

        removed = ignoredUrlsA;
        added = ignoredUrlsB;
        for (auto it = removed.begin(); it != removed.end(); /* no inc */) {
            nx::Url url = *it;
            nx::Url implicitUrl = url;
            implicitUrl.setPort(-1);

            if (added.contains(url) || added.contains(implicitUrl)) {
                added.remove(url);
                added.remove(implicitUrl);
                it = removed.erase(it);
            } else {
                ++it;
            }
        }

        for (const nx::Url &url: removed)
            change.ignoredAddresses.insert(url, false);
        for (const nx::Url &url: added)
            change.ignoredAddresses.insert(url, true);

        return change;
    }

private:
    void processSet(QSet<nx::Url> &set, const QHash<nx::Url, bool> &hash) const {
        for (auto it = hash.begin(); it != hash.end(); ++it) {
            if (it.value())
                set.insert(it.key());
            else
                set.remove(it.key());
        }
    }
};

class RoutingManagementChanges {
public:
    QHash<nx::Uuid, RoutingChange> changes;
};

QnRoutingManagementWidget::QnRoutingManagementWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnRoutingManagementWidget),
    m_changes(new RoutingManagementChanges)
{
    ui->setupUi(this);

    ui->addressesView->setItemDelegateForColumn(QnServerAddressesModel::InUseColumn,
        new SwitchItemDelegate(this));

    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, HelpTopic::Id::Administration_RoutingManagement);

    m_serverListModel = new QnResourceListModel(this);
    m_serverListModel->setReadOnly(true);

    SortedServersProxyModel *sortedServersModel = new SortedServersProxyModel(this);
    sortedServersModel->setSourceModel(m_serverListModel);
    sortedServersModel->setDynamicSortFilter(true);
    sortedServersModel->setSortRole(Qt::DisplayRole);
    sortedServersModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    sortedServersModel->sort(Qn::NameColumn);
    ui->serversView->setModel(sortedServersModel);

    m_serverAddressesModel = new QnServerAddressesModel(this);
    m_sortedServerAddressesModel = new QnSortedServerAddressesModel(this);
    m_sortedServerAddressesModel->setDynamicSortFilter(true);
    m_sortedServerAddressesModel->setSortRole(Qt::DisplayRole);
    m_sortedServerAddressesModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sortedServerAddressesModel->sort(QnServerAddressesModel::AddressColumn);
    m_sortedServerAddressesModel->setSourceModel(m_serverAddressesModel);
    ui->addressesView->setModel(m_sortedServerAddressesModel);
    ui->addressesView->header()->setSectionResizeMode(QnServerAddressesModel::AddressColumn, QHeaderView::Stretch);
    ui->addressesView->header()->setSectionResizeMode(QnServerAddressesModel::InUseColumn, QHeaderView::ResizeToContents);
    ui->addressesView->header()->setSectionsMovable(false);

    ui->addressesView->setDefaultSpacePressIgnored(true);
    ui->addressesView->setMultiSelectionEditAllowed(true);
    connect(ui->addressesView, &TreeView::spacePressed, this,
        [this](const QModelIndex& index)
        {
            auto checkIndex = index.sibling(index.row(), QnServerAddressesModel::InUseColumn);
            auto checkState = static_cast<Qt::CheckState>(checkIndex.data(Qt::CheckStateRole).toInt());
            auto newState = checkState == Qt::Checked ? Qt::Unchecked : Qt::Checked;
            ui->addressesView->model()->setData(checkIndex, newState, Qt::CheckStateRole);
        });

    SnappedScrollBar *scrollBar = new SnappedScrollBar(this);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->addressesView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(ui->serversView->selectionModel(),  &QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::at_serversView_currentIndexChanged);
    connect(ui->addressesView->selectionModel(),&QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::updateUi);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::dataChanged,           this,   &QnRoutingManagementWidget::at_serverAddressesModel_dataChanged);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::urlEditingFailed,      this,   [this](const QModelIndex &, int error) { reportUrlEditingError(error); });
    connect(ui->addButton,                      &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_addButton_clicked);
    connect(ui->removeButton,                   &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_removeButton_clicked);

    connect(resourcePool(),  &QnResourcePool::resourceAdded,     this,   &QnRoutingManagementWidget::at_resourcePool_resourceAdded);
    connect(resourcePool(),  &QnResourcePool::resourceRemoved,   this,   &QnRoutingManagementWidget::at_resourcePool_resourceRemoved);

    m_serverListModel->setResources(resourcePool()->getResourcesWithFlag(Qn::server));

    updateUi();

    /* Immediate selection screws up initial size, so update on 1st show: */
    installEventHandler(this, QEvent::Show, this,
        [this]()
        {
            if (!ui->serversView->currentIndex().isValid())
                ui->serversView->setCurrentIndex(ui->serversView->model()->index(0, 0));
        });
}

QnRoutingManagementWidget::~QnRoutingManagementWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void QnRoutingManagementWidget::loadDataToUi() {
    m_changes->changes.clear();
    ui->warningLabel->hide();
    updateModel();
    updateUi();
}

void QnRoutingManagementWidget::applyChanges() {
    ui->warningLabel->hide();
    if (isReadOnly())
        return;

    auto connection = messageBusConnection();
    if (!connection)
        return;

    if (!NX_ASSERT(m_requests.empty()))
        return;

    updateFromModel();

    auto requestCompletionHandler = nx::utils::guarded(this,
        [this](int requestId, ec2::ErrorCode /*errorCode*/)
        {
            NX_ASSERT(m_requests.contains(requestId) || m_requests.empty());
            m_requests.remove(requestId);
        });

    for (auto it = m_changes->changes.begin(); it != m_changes->changes.end(); ++it) {
        nx::Uuid serverId = it.key();
        QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
        if (!server)
            continue;

        QSet<nx::Url> autoUrls;
        QSet<nx::Url> additionalUrls;
        QSet<nx::Url> ignoredUrls;
        getAddresses(server, autoUrls, additionalUrls, ignoredUrls);

        it->simplify(autoUrls, additionalUrls, ignoredUrls, server->getPort());
        QHash<nx::Url, bool> additional = it->addresses;
        QHash<nx::Url, bool> ignored = it->ignoredAddresses;

        const auto discoveryManager = connection->getDiscoveryManager(nx::network::rest::kSystemSession);
        for (auto it = additional.begin(); it != additional.end(); ++it) {
            nx::Url url = it.key();

            if (it.value()) {
                bool ign = false;
                if (ignored.contains(url))
                    ign = ignored.take(url);

                const auto requestId = discoveryManager->addDiscoveryInformation(
                    serverId, url, ign, requestCompletionHandler);
                if (requestId > 0)
                    m_requests.insert(requestId);
            } else {
                ignored.remove(url);
                const auto requestId = discoveryManager->removeDiscoveryInformation(
                    serverId, url, false, requestCompletionHandler);
                if (requestId > 0)
                    m_requests.insert(requestId);
            }
        }
        for (auto it = ignored.begin(); it != ignored.end(); ++it) {
            nx::Url url = it.key();

            if (it.value())
            {
                const auto requestId = discoveryManager->addDiscoveryInformation(
                    serverId, url, true, requestCompletionHandler);
                if (requestId > 0)
                    m_requests.insert(requestId);
            }
            else if (autoUrls.contains(url))
            {
                const auto requestId = discoveryManager->removeDiscoveryInformation(
                    serverId, url, false, requestCompletionHandler);
                if (requestId > 0)
                    m_requests.insert(requestId);
            }
            else
            {
                const auto requestId = discoveryManager->addDiscoveryInformation(
                    serverId, url, false, requestCompletionHandler);
                if (requestId > 0)
                    m_requests.insert(requestId);
            }
        }
    }

    m_changes->changes.clear();
}

void QnRoutingManagementWidget::discardChanges()
{
    // #TODO: sivanov Implement correct requests cancellation after switch to REST API.
    m_requests.clear();
}

bool QnRoutingManagementWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return std::any_of(
        m_changes->changes.cbegin(),
        m_changes->changes.cend(),
        [](const auto& change) { return !change.isEmpty(); });
}

bool QnRoutingManagementWidget::isNetworkRequestRunning() const
{
    return !m_requests.empty();
}

void QnRoutingManagementWidget::setReadOnlyInternal(bool readOnly) {
    using ::setReadOnly;

    setReadOnly(ui->addButton, readOnly);
    setReadOnly(ui->removeButton, readOnly);
    m_serverAddressesModel->setReadOnly(readOnly);
    updateUi();
}

void QnRoutingManagementWidget::updateModel() {
    if (!m_server) {
        m_serverAddressesModel->clear();
        return;
    }

    nx::Uuid serverId = m_server->getId();

    QSet<nx::Url> autoUrls;
    QSet<nx::Url> additionalUrls;
    QSet<nx::Url> ignoredUrls;
    getAddresses(m_server, autoUrls, additionalUrls, ignoredUrls);

    m_changes->changes.value(serverId).apply(additionalUrls, ignoredUrls);

    int row = ui->addressesView->currentIndex().row();
    nx::Url url = nx::Url::fromQUrl(ui->addressesView->currentIndex().data(Qt::EditRole).toUrl());

    m_serverAddressesModel->resetModel(autoUrls.values(), additionalUrls.values(), ignoredUrls, m_server->getPort());

    int rowCount = m_serverAddressesModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QModelIndex index = m_serverAddressesModel->index(i, QnServerAddressesModel::AddressColumn);
        if (m_serverAddressesModel->data(index, Qt::EditRole).toUrl() == url.toQUrl()) {
            row = m_sortedServerAddressesModel->mapFromSource(index).row();
            break;
        }
    }

    if (row < m_sortedServerAddressesModel->rowCount())
        ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));

    emit hasChangesChanged();
}

void QnRoutingManagementWidget::updateFromModel() {
    if (!m_server)
        return;

    nx::Uuid serverId = m_server->getId();

    QSet<nx::Url> autoUrls;
    QSet<nx::Url> additionalUrls;
    QSet<nx::Url> ignoredUrls;
    getAddresses(m_server, autoUrls, additionalUrls, ignoredUrls);
    m_changes->changes[serverId] = RoutingChange::diff(
        additionalUrls, ignoredUrls,
        nx::utils::toQSet(m_serverAddressesModel->manualAddressList()),
        m_serverAddressesModel->ignoredAddresses());
    emit hasChangesChanged();
}

void QnRoutingManagementWidget::updateUi() {
    QModelIndex sourceIndex = m_sortedServerAddressesModel->mapToSource(ui->addressesView->currentIndex());

    ui->buttonsWidget->setVisible(ui->serversView->currentIndex().isValid() && !isReadOnly());
    ui->removeButton->setVisible(m_serverAddressesModel->isManualAddress(sourceIndex) && !isReadOnly());
}

quint16 QnRoutingManagementWidget::getCurrentServerPort()
{
    const auto address = m_server->getPrimaryAddress();
    const bool isUsualHost = !nx::network::SocketGlobals::addressResolver()
        .isCloudHostname(address.address.toString());

    if (isUsualHost && (address.port > 0))
        return address.port;

    const auto addresses = m_serverAddressesModel->addressList();
    if (addresses.isEmpty())
        return helpers::kDefaultConnectionPort;

    const auto currentPort = addresses.first().port();
    if (currentPort > 0)
        return currentPort;

    return helpers::kDefaultConnectionPort;
}

void QnRoutingManagementWidget::at_addButton_clicked() {
    if (!m_server)
        return;

    QString urlString = QnInputDialog::getText(this, tr("Enter URL"), tr("URL"));
    if (urlString.isEmpty())
        return;

    nx::Url url = nx::Url::fromUserInput(urlString);
    url.setScheme(nx::network::http::kSecureUrlSchemeName);

    const bool validUrl = url.isValid() && !url.host().isEmpty();
    if (!validUrl)
    {
        reportUrlEditingError(QnServerAddressesModel::InvalidUrl);
        return;
    }

    if (url.port() <= 0)
        url.setPort(getCurrentServerPort());

    nx::Url implicitUrl = url;
    implicitUrl.setPort(-1);
    if (m_serverAddressesModel->addressList().contains(url) ||
        m_serverAddressesModel->addressList().contains(implicitUrl) ||
        m_serverAddressesModel->manualAddressList().contains(url) ||
        m_serverAddressesModel->manualAddressList().contains(implicitUrl))
    {
        reportUrlEditingError(QnServerAddressesModel::ExistingUrl);
        return;
    }

    m_serverAddressesModel->addAddress(url);

    updateFromModel();
}

void QnRoutingManagementWidget::at_removeButton_clicked() {
    QModelIndex currentIndex = ui->addressesView->currentIndex();

    QModelIndex index = m_sortedServerAddressesModel->mapToSource(currentIndex);
    if (!index.isValid())
        return;

    if (!m_server)
        return;

    m_serverAddressesModel->removeAddressAtIndex(index);

    int row = qMin(m_sortedServerAddressesModel->rowCount() - 1, currentIndex.row());
    ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));

    updateFromModel();
}

void QnRoutingManagementWidget::at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous);

    if (m_server)
        m_server->disconnect(this);

    updateFromModel();

    QnMediaServerResourcePtr server =
        current.data(ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
    if (server == m_server)
        return;

    m_server = server;
    updateModel();
    updateUi();

    if (server)
    {
        connect(server.get(), &QnMediaServerResource::resourceChanged, this, &QnRoutingManagementWidget::updateModel);
        connect(server.get(), &QnMediaServerResource::auxUrlsChanged, this, &QnRoutingManagementWidget::updateModel);
    }
}

void QnRoutingManagementWidget::at_serverAddressesModel_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    if (!m_server)
        return;

    if (topLeft != bottomRight || topLeft.column() != QnServerAddressesModel::InUseColumn)
        return;

    updateFromModel();

    if ((topLeft.data(Qt::CheckStateRole).toInt() == Qt::Unchecked) && (!m_serverAddressesModel->isManualAddress(topLeft))) {
        ui->warningLabel->setVisible(hasChanges());
    } else if (!hasChanges()) {
        ui->warningLabel->setVisible(false);
    }
}

void QnRoutingManagementWidget::reportUrlEditingError(int error) {
    switch (error) {
    case QnServerAddressesModel::InvalidUrl:
        QnMessageBox::warning(this, tr("Invalid URL"));
        break;
    case QnServerAddressesModel::ExistingUrl:
        QnMessageBox::information(this, tr("URL already added"));
        break;
    }
}

void QnRoutingManagementWidget::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    m_serverListModel->addResource(resource);
}

void QnRoutingManagementWidget::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    m_serverListModel->removeResource(resource);

    if (m_server == resource)
        updateModel();
}
