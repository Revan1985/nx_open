// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "welcome_screen.h"

#include <QtCore/QMimeData>
#include <QtGui/QAction>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QApplication>

#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <client/forgotten_systems_manager.h>
#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_to_cloud_tool.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/models/systems_controller.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/util.h>

#include "../data/connect_tiles_proxy_model.h"
#include "../data/systems_visibility_sort_filter_model.h"
#include "setup_wizard_dialog.h"

using CredentialsManager = nx::vms::client::core::CredentialsManager;
using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;

namespace {

const int kDefaultSimpleModeTilesNumber = 6;
const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(12);

QnResourceList extractResources(const QList<QUrl>& urls, QnResourcePool* resourcePool)
{
    return QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(urls),
        resourcePool);
}

} // namespace

namespace nx::vms::client::desktop {

using core::CloudStatusWatcher;

WelcomeScreen::WelcomeScreen(WindowContext* context, QWidget* parent):
    base_type(appContext()->qmlEngine(), parent),
    WindowContextAware(context)
{
    NX_ASSERT(qnRuntime->isDesktopMode());

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark3"));

    rootContext()->setContextProperty("workbench", workbench());
    rootContext()->setContextProperty(lit("context"), this);
    setSource(lit("Nx/WelcomeScreen/WelcomeScreen.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (status() == QQuickWidget::Error)
    {
        for (const auto& error: errors())
            NX_ERROR(this, error.toString());
        NX_CRITICAL(false, "Welcome screen loading failed.");
    }

    NX_CRITICAL(qnCloudStatusWatcher, "Cloud watcher does not exist");
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudLoginChanged,
        this, &WelcomeScreen::cloudUserNameChanged);
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::statusChanged, this,
        [this]()
        {
            if (m_systemModel)
            {
                m_systemModel->setLoggedToCloud(
                    qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut);
            }
            emit hasCloudConnectionIssueChanged();
        });
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::isCloudEnabledChanged,
        this, &WelcomeScreen::isCloudEnabledChanged);

    connect(qnCloudStatusWatcher, &CloudStatusWatcher::is2FaEnabledForUserChanged,
        this, &WelcomeScreen::is2FaEnabledForUserChanged);

    setHelpTopic(this, HelpTopic::Id::Login);

    connect(&appContext()->localSettings()->autoLogin,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &WelcomeScreen::resetAutoLogin);

    createSystemModel();
}

void WelcomeScreen::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);

    // Resize offscreen QQuickWindow content item here because
    // it does not get resized when offscreen QQuickWindow size is changed.
    // This content item is a parent for popup mouse grab overlay.
    // The overlay should be the same size as the window in order to
    // correctly hide popups when mouse button is pressed.
    if (auto contentItem = quickWindow()->contentItem())
        contentItem->setSize(quickWindow()->size());
}

WelcomeScreen::~WelcomeScreen()
{
    for (auto request: m_runningRequests)
        request->pleaseStopSync();
    m_runningRequests.clear();
}

void WelcomeScreen::connectionToSystemEstablished(const QString& systemId)
{
    NX_VERBOSE(this, "Connected to system: %1", systemId);

    emit closeTile(systemId);
}

QObject* WelcomeScreen::systemModel()
{
    return m_systemModel;
}

void WelcomeScreen::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    action(menu::EscapeHotkeyAction)->setEnabled(false);
    emit gridEnabledChanged();
}

void WelcomeScreen::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);

    setGlobalPreloaderVisible(false); //< Auto toggle off preloader

    action(menu::EscapeHotkeyAction)->setEnabled(true);
    emit gridEnabledChanged();
}

void WelcomeScreen::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        quickWindow()->setColor(palette().color(QPalette::Window));

    base_type::changeEvent(event);
}

bool WelcomeScreen::hasCloudConnectionIssue() const
{
    return qnCloudStatusWatcher->status() == CloudStatusWatcher::Status::Offline;
}

QString WelcomeScreen::cloudUserName() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

bool WelcomeScreen::isCloudEnabled() const
{
    return qnCloudStatusWatcher->isCloudEnabled();
}

bool WelcomeScreen::visibleControls() const
{
    return m_visibleControls;
}

void WelcomeScreen::setVisibleControls(bool visible)
{
    if (m_visibleControls == visible)
        return;

    m_visibleControls = visible;
    emit visibleControlsChanged();
}

bool WelcomeScreen::gridEnabled() const
{
    return !isHidden();
}

void WelcomeScreen::openConnectingTile(std::optional<RemoteConnectionErrorCode> errorCode)
{
    const auto systemId = m_connectingSystemId;

    QString errorMessage;

    // If this parameter is true, login field will be highlighted with a red frame, otherwise it
    // will be the host field.
    const bool isLoginError = !systemId.isEmpty()
        && errorCode
        && (
            *errorCode == RemoteConnectionErrorCode::unauthorized
            || *errorCode == RemoteConnectionErrorCode::loginAsCloudUserForbidden
            || *errorCode == RemoteConnectionErrorCode::sessionExpired
            || *errorCode == RemoteConnectionErrorCode::cloudSessionExpired
            || *errorCode == RemoteConnectionErrorCode::userIsLockedOut
            || *errorCode == RemoteConnectionErrorCode::userIsDisabled
            || *errorCode == RemoteConnectionErrorCode::truncatedSessionToken
            );

    m_connectingSystemId.clear(); //< Connection process is finished.

    if (!systemId.isEmpty() && errorCode)
        errorMessage = core::shortErrorDescription(*errorCode);

    executeLater(
        [this, systemId, errorMessage, isLoginError]()
        {
            NX_VERBOSE(this, "Open tile, system: %1, login error: %2", systemId, isLoginError);
            emit openTile(systemId, errorMessage, isLoginError);
        },
        this);
}

void WelcomeScreen::openArbitraryTile(const QString& systemId)
{
    if (!NX_ASSERT(!systemId.isEmpty()))
        return;

    executeLater(
        [this, systemId]()
        {
            const auto indexes = m_systemModel->match(m_systemModel->index(0, 0),
                QnSystemsModel::SystemIdRoleId, systemId, 1, Qt::MatchFixedString);

            if (!indexes.empty())
                emit openTileByIndex(indexes[0].row());
        },
        this);
}

bool WelcomeScreen::connectingTileExists() const
{
    return !m_connectingSystemId.isEmpty();
}

void WelcomeScreen::setConnectingToSystem(const QString& value)
{
    NX_VERBOSE(this, "Connecting to system: %1", value);

    m_connectingSystemId = value;
}

bool WelcomeScreen::globalPreloaderVisible() const
{
    return m_globalPreloaderVisible;
}

void WelcomeScreen::setGlobalPreloaderVisible(bool value)
{
    if (value == m_globalPreloaderVisible)
        return;

    NX_VERBOSE(this, "Set preloader visible: %1", value);

    m_globalPreloaderVisible = value;
    emit globalPreloaderVisibleChanged(value);
}

void WelcomeScreen::setMessage(const QString& message)
{
    if (m_message == message)
        return;

    m_message = message;

    emit messageChanged();

    // Repainting the widget to guarantee that started screen recording won't contain the visible
    // message.
    // TODO: Find a better way to achieve the same effect.
    update();
    repaint();

    qApp->sendPostedEvents();
}

QString WelcomeScreen::message() const
{
    return m_message;
}

int WelcomeScreen::simpleModeTilesNumber() const
{
    return ini().simpleModeTilesNumber == -1
        ? kDefaultSimpleModeTilesNumber
        : ini().simpleModeTilesNumber;
}

bool WelcomeScreen::richTextEnabled() const
{
    return ini().developerMode;
}

bool WelcomeScreen::confirmCloudTileHiding() const
{
    auto dialog = new QnMessageBox(
        QnMessageBox::Icon::Question,
        tr("Are you sure you want to hide this tile?"),
        tr("You will be able to access the %1 login menu at any time by clicking "
           "the cloud icon on the navigation panel.").arg(nx::branding::cloudName()),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parentWidget());
    dialog->addButton(tr("Hide"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

    return dialog->exec() != QDialogButtonBox::Cancel;
}

void WelcomeScreen::openHelp() const
{
    HelpHandler::openHelpTopic(HelpTopic::Id::MainWindow_TitleBar_MainMenu);
}

QString WelcomeScreen::minSupportedVersion() const
{
    return nx::vms::common::ServerCompatibilityValidator::minimalVersion().toString();
}

void WelcomeScreen::showSystemWentToOfflineNotification() const
{
    // Synchronous call leads to potential QML binding loops.
    executeDelayed(
        [this]
        {
            QnMessageBox::critical(parentWidget(),
                tr("Unable to connect to the site because it has become offline"));
            dropConnectingState();
        });
}

bool WelcomeScreen::checkUrlIsValid(const QString& urlText) const
{
    QUrl url(urlText);
    return url.isValid() && !url.scheme().isEmpty();
}

void WelcomeScreen::abortConnectionProcess()
{
    NX_ASSERT(!m_connectingSystemId.isEmpty());
    m_connectingSystemId.clear();
    menu()->trigger(menu::DisconnectAction, {Qn::ForceRole, true});
}

void WelcomeScreen::connectToLocalSystem(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId,
    const QString& userName,
    const QString& password,
    bool storePassword)
{
    nx::Url url = parseConnectionUrlFromUserInput(serverUrl);

    connectToSystemInternal(
        systemId,
        serverId.isEmpty() ? std::nullopt : std::optional{nx::Uuid::fromStringSafe(serverId)},
        nx::network::SocketAddress(url.host(), url.port()),
        nx::network::http::PasswordCredentials{userName.toStdString(), password.toStdString()},
        storePassword);
}

void WelcomeScreen::connectToLocalSystemUsingSavedPassword(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId,
    const QString& userName,
    bool storePassword)
{
    const auto system = appContext()->systemFinder()->getSystem(systemId);
    if (!NX_ASSERT(system, "System is empty"))
        return;

    const nx::Uuid localSystemId = system->localId();
    const auto credentials = CredentialsManager::credentials(
        localSystemId,
        userName.toStdString());
    const nx::Url url = parseConnectionUrlFromUserInput(serverUrl);

    connectToSystemInternal(
        systemId,
        serverId.isEmpty() ? std::nullopt : std::optional{nx::Uuid::fromStringSafe(serverId)},
        nx::network::SocketAddress::fromUrl(url),
        credentials.value_or(network::http::Credentials{}),
        storePassword);
}

void WelcomeScreen::forgetUserPassword(
    const QString& localSystemId,
    const QString& user)
{
    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    if (!localId.isNull() && !user.isEmpty())
        CredentialsManager::forgetStoredPassword(localId, user.toStdString());
}

void WelcomeScreen::forgetAllCredentials(const QString& localSystemId)
{
    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    if (!localId.isNull())
        CredentialsManager::removeCredentials(localId);
}

void WelcomeScreen::forceActiveFocus()
{
    if (const auto rootItem = rootObject())
        rootItem->forceActiveFocus();
}

bool WelcomeScreen::isLoggedInToCloud() const
{
    return (qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut);
}

static SystemsVisibilitySortFilterModel* createVisibilityModel(QObject* parent)
{
    using TileScopeFilter = nx::vms::client::core::welcome_screen::TileScopeFilter;

    auto visibilityModel = new SystemsVisibilitySortFilterModel(parent);

    auto forgottenSystemsManager = appContext()->forgottenSystemsManager();

    NX_CRITICAL(forgottenSystemsManager,
        "Client forgotten systems manager is not initialized yet");

    visibilityModel->setVisibilityScopeFilterCallbacks(
        []()
        {
            return static_cast<TileScopeFilter>(
                appContext()->coreSettings()->tileVisibilityScopeFilter());
        },
        [](TileScopeFilter filter)
        {
            appContext()->coreSettings()->tileVisibilityScopeFilter = filter;
        }
    );

    visibilityModel->setForgottenCheckCallback(
        [forgottenSystemsManager](const QString& systemId)
        {
            return forgottenSystemsManager->isForgotten(systemId);
        }
    );

    QObject::connect(forgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemAdded,
        visibilityModel, &SystemsVisibilitySortFilterModel::forgottenSystemAdded);

    QObject::connect(forgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemRemoved,
        visibilityModel, &SystemsVisibilitySortFilterModel::forgottenSystemRemoved);

    visibilityModel->setSourceModel(
        new QnSystemsModel(new SystemsController(visibilityModel), visibilityModel));

    return visibilityModel;
}

void WelcomeScreen::createSystemModel()
{
    using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

    m_systemModel = new ConnectTilesProxyModel(this);

    m_systemModel->setCloudVisibilityScopeCallbacks(
        []()
        {
            return qnSystemsVisibilityManager->cloudTileScope();
        },
        [](TileVisibilityScope value)
        {
            qnSystemsVisibilityManager->setCloudTileScope(value);
        }
    );

    m_systemModel->setSourceModel(createVisibilityModel(m_systemModel));

    m_systemModel->setLoggedToCloud(
        qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut);

    emit systemModelChanged();
}

void WelcomeScreen::connectToSystemInternal(
    const QString& systemId, //< Actually that's a tile id.
    const std::optional<nx::Uuid>& serverId,
    const nx::network::SocketAddress& address,
    const nx::network::http::Credentials& credentials,
    bool storePassword,
    const nx::utils::SharedGuardPtr& completionTracker)
{
    if (!m_connectingSystemId.isEmpty())
        return; //< Connection process is in progress

    NX_DEBUG(this, "Delayed connect to the system %1 after click on tile", systemId);
    LogonData logonData(core::LogonData{
        .address = address,
        .credentials = credentials,
        .expectedServerId = serverId});
    logonData.storePassword = storePassword;
    logonData.connectScenario = ConnectScenario::connectFromTile;

    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction =
        [this, systemId, logonData = std::move(logonData), completionTracker]()
        {
            NX_DEBUG(this, "Connecting to the system: %1, address: %2, server: %3, scenario: %4",
                systemId,
                logonData.address,
                logonData.expectedServerId,
                logonData.connectScenario);

            if (!NX_ASSERT(logonData.address.port && !logonData.address.address.isEmpty(),
                "Wrong system address: %1", logonData.address))
            {
                emit dropConnectingState();
                return;
            }

            setConnectingToSystem(systemId);

            menu()->trigger(menu::ConnectAction,
                menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
        };

    // We have to use delayed execution to prevent client crash when stopping server that we are
    // connecting to.
    executeLater(connectFunction, this);
}

void WelcomeScreen::connectToCloudSystem(const QString& systemId)
{
    if (!isLoggedInToCloud())
        return;

    setConnectingToSystem(systemId);
    CloudSystemConnectData connectData{systemId, ConnectScenario::connectFromTile};
    menu()->trigger(menu::ConnectToCloudSystemAction,
        menu::Parameters().withArgument(Qn::CloudSystemConnectDataRole, connectData));
}

void WelcomeScreen::connectToAnotherSystem()
{
    menu()->trigger(menu::OpenLoginDialogAction);
}

void WelcomeScreen::testSystemOnline(const QString& serverUrl)
{
    if (const auto manager = appContext()->moduleDiscoveryManager())
    {
        const auto url = parseConnectionUrlFromUserInput(serverUrl);
        if (url.isValid())
            manager->checkEndpoint(url);
    }
}

void WelcomeScreen::setupFactorySystem(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId)
{
    connectToLocalSystem(
        systemId,
        serverUrl,
        serverId,
        helpers::kFactorySystemUser,
        helpers::kFactorySystemPassword,
        /*storePassword*/ false);
}

void WelcomeScreen::setupFactorySystem(
    const nx::network::SocketAddress& address,
    const nx::Uuid& serverId)
{
    setVisibleControls(false);

    auto bindToCloudWhenReady =
        [this](SystemContext* context)
        {
            if (!NX_ASSERT(context))
                return;

            connect(context->messageProcessor(),
                &QnCommonMessageProcessor::initialResourcesReceived,
                this,
                [this, context]()
                {
                    auto connectTool = new ConnectToCloudTool(this, context->globalSettings());
                    connect(connectTool, &ConnectToCloudTool::finished,
                        connectTool, &QObject::deleteLater);

                    connectTool->start();
                },
                Qt::SingleShotConnection);
        };

    const auto controlsGuard = nx::utils::makeSharedGuard(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler =
        [this, address, serverId, controlsGuard, bindToCloudWhenReady]()
        {
            /* We are receiving string with port but without protocol, so we must parse it. */
            const auto dialog = new SetupWizardDialog(address, serverId, mainWindowWidget());
            dialog->setAttribute(Qt::WA_DeleteOnClose);

            const auto connectToSystem =
                [this, dialog, address, serverId, controlsGuard, bindToCloudWhenReady]()
                {
                    const auto credentials = nx::network::http::PasswordCredentials(
                        helpers::kFactorySystemUser.toStdString(),
                        dialog->password().toStdString());

                    if (dialog->isBindingToCloudRequested())
                        bindToCloudWhenReady(system());

                    connectToSystemInternal(
                        /*systemId*/ QString(),
                        serverId,
                        address,
                        credentials,
                        /*storePassword*/ false,
                        controlsGuard);
                };

            connect(dialog, &QDialog::accepted, this, connectToSystem);

            if (!ini().modalServerSetupWizard)
            {
                dialog->loadPage();
                dialog->show();
            }
            else
            {
                dialog->exec();
            }
        };

    // Use delayed handling for proper animation
    static const int kNextEventDelayMs = 100;
    executeDelayedParented(showDialogHandler, kNextEventDelayMs, this);
}

void WelcomeScreen::logoutFromCloud()
{
    menu()->trigger(menu::LogoutFromCloud);
}

void WelcomeScreen::manageCloudAccount()
{
    menu()->trigger(menu::OpenCloudManagementUrl);
}

void WelcomeScreen::loginToCloud()
{
    menu()->trigger(menu::LoginToCloud);
}

void WelcomeScreen::createAccount()
{
    menu()->trigger(menu::OpenCloudRegisterUrl);
}

void WelcomeScreen::deleteSystem(const QString& systemId, const QString& localSystemId)
{
    const auto localSystemUuid = nx::Uuid::fromStringSafe(localSystemId);
    NX_ASSERT(!localSystemUuid.isNull());
    if (localSystemUuid.isNull())
        return;

    auto forgottenSystemsManager = appContext()->forgottenSystemsManager();
    forgottenSystemsManager->forgetSystem(systemId);
    forgottenSystemsManager->forgetSystem(localSystemId);

    CredentialsManager::removeCredentials(localSystemUuid);
    appContext()->coreSettings()->removeRecentConnection(localSystemUuid);
    qnSystemsVisibilityManager->removeSystemData(localSystemUuid);
    core::appContext()->knownServerConnectionsWatcher()->removeSystem(systemId);
}

bool WelcomeScreen::saveCredentialsAllowed() const
{
    return appContext()->localSettings()->saveCredentialsAllowed();
}

bool WelcomeScreen::is2FaEnabledForUser() const
{
    return qnCloudStatusWatcher->is2FaEnabledForUser();
}

} // namespace nx::vms::client::desktop
