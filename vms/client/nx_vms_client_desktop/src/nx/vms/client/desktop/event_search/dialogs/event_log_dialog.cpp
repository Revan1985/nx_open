// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"

#include <chrono>

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QMenu>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/analytics_event.h>
#include <nx/vms/rules/group.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/type.h>
#include <nx/vms/time/formatter.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "../models/event_log_model.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

using namespace nx::vms::rules;

namespace {

constexpr auto kUpdateDelay = 1s;
constexpr auto kQueryTimeout = 15s;
constexpr auto kMaxEventLogToRequest = 1000;

const auto kAnalyticsEventType = rules::utils::type<AnalyticsEvent>();

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight16Substitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light17"}},
};

static const QColor klight10Color = "#A5B7C0";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight10Color, "light10"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{klight10Color, "light11"}, {kLight16Color, "light17"}}},
    {QIcon::Selected, {{klight10Color, "light9"}}},
    {QnIcon::Error, {{klight10Color, "red"}}},
};

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight12Theme = {
    {QnIcon::Normal, {.primary = "light12"}},
};

NX_DECLARE_COLORIZED_ICON(kReloadIcon, "20x20/Outline/reload.svg", kLight16Substitutions)
NX_DECLARE_COLORIZED_ICON(kEventRulesIcon, "20x20/Outline/event_rules.svg", kLight12Theme)

enum EventListRoles
{
    EventTypeRole = Qt::UserRole + 1,
    AnalyticsEventTypeIdRole,
};

enum ActionListRoles
{
    ActionTypeRole = Qt::UserRole + 1,
    ProlongedOnlyActionRole, // The action can't connect to instant only events.
};

QString actionType(const QModelIndex& index)
{
    return index.isValid()
        ? index.data(ActionTypeRole).toString()
        : QString();
}

QString eventType(const QModelIndex& index)
{
    return index.isValid()
        ? index.data(EventTypeRole).toString()
        : QString();
}

std::optional<nx::vms::api::json::ValueOrArray<QString>> typeList(const QString& type)
{
    if (type.isEmpty())
        return std::nullopt;

    return type;
}

nx::vms::api::analytics::EventTypeId analyticsEventTypeId(const QModelIndex& index)
{
    return index.isValid()
        ? index.data(AnalyticsEventTypeIdRole).value<nx::vms::api::analytics::EventTypeId>()
        : nx::vms::api::analytics::EventTypeId();
}

QModelIndex analyticsEventsRoot(QStandardItemModel* model)
{
    const auto indices = model->match(
        model->index(0, 0),
        EventTypeRole,
        /*value*/ kAnalyticsEventType,
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    NX_ASSERT(indices.size() == 1);
    if (indices.size() != 1)
        return QModelIndex();

    return indices[0];
}

// Returns width for the longest(symbols count) available date.
int dateTimeColumnDefaultWidth(QTableView* tableView)
{
    static const auto datetimeString =
        time::toString(QDateTime::fromString("12/12/2024 12:12:12", "dd/MM/yyyy hh:mm:ss"));

    // The returned length in pixels is large enough(with some extra) to accommodate any possible
    // datetime string.
    return tableView->fontMetrics().averageCharWidth() * datetimeString.size()
        + style::Metrics::kStandardPadding * 2;
}

int calculateItemMaxWidth(
    const QFontMetrics& metrics, const QMap<QString, ItemDescriptor>& items)
{
    qsizetype maxCharCount = 0;
    for (auto& item: items)
        maxCharCount = std::max(maxCharCount, item.displayName.value().size());

    return metrics.averageCharWidth() * maxCharCount + style::Metrics::kStandardPadding * 2;
}

} // namespace

enum class EventLogDialog::State
{
    loading,
    loaded,
    error,
};

EventLogDialog::EventLogDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::EventLogDialog),
    m_eventTypesModel(new QStandardItemModel()),
    m_actionTypesModel(new QStandardItemModel()),
    m_lastMouseButton(Qt::NoButton)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, HelpTopic::Id::MainWindow_Notifications_EventLog);

    constexpr auto kDefaultSort = std::make_pair(
        EventLogModel::DateTimeColumn, Qt::DescendingOrder);

    m_model = new EventLogModel(system(), this);
    m_model->setColumns({
        EventLogModel::DateTimeColumn,
        EventLogModel::EventColumn,
        EventLogModel::EventSourceColumn,
        EventLogModel::ActionColumn,
        EventLogModel::ActionCameraColumn,
        EventLogModel::DescriptionColumn});
    m_model->sort(kDefaultSort.first, kDefaultSort.second);

    ui->gridEvents->setModel(m_model);
    ui->gridEvents->hoverTracker()->setMouseCursorRole(Qn::ItemMouseCursorRole);
    ui->gridEvents->horizontalHeader()->setSortIndicator(
        kDefaultSort.first, kDefaultSort.second);

    initEventsModel();
    initActionsModel();

    connect(windowContext(), &WindowContext::systemChanged, this, &EventLogDialog::retranslateUi);
    retranslateUi();

    m_filterAction      = new QAction(tr("Filter Similar Rows"), this);
    m_filterAction->setShortcut(QKeySequence::Find);
    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    m_resetFilterAction = new QAction(tr("Clear Filter"), this);
    // TODO: #sivanov shouldn't we use QKeySequence::Refresh instead (evaluates to F5 on win)?
    m_resetFilterAction->setShortcut({Qt::ControlModifier | Qt::Key_R});

    installEventHandler(ui->gridEvents->viewport(), QEvent::MouseButtonRelease,
        this, &EventLogDialog::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);
    ui->gridEvents->addAction(m_filterAction);
    ui->gridEvents->addAction(m_resetFilterAction);

    ui->clearFilterButton->setIcon(
        qnSkin->icon("20x20/Outline/close_medium.svg", kLight16Substitutions));
    connect(ui->clearFilterButton, &QPushButton::clicked, this, &EventLogDialog::reset);

    ui->refreshButton->setIcon(qnSkin->icon(kReloadIcon));
    ui->eventRulesButton->setIcon(qnSkin->icon(kEventRulesIcon));

    auto scrollBar = new SnappedScrollBar(this);
    ui->gridEvents->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(m_filterAction, &QAction::triggered, this, &EventLogDialog::at_filterAction_triggered);
    connect(m_resetFilterAction, &QAction::triggered, this, &EventLogDialog::reset);
    connect(m_clipboardAction, &QAction::triggered,
        this, &EventLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction, &QAction::triggered, this, &EventLogDialog::at_exportAction_triggered);
    connect(m_selectAllAction, &QAction::triggered, ui->gridEvents, &QTableView::selectAll);

    connect(ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged,
        this, &EventLogDialog::updateData);

    connect(ui->eventComboBox, QnComboboxCurrentIndexChanged, this, &EventLogDialog::updateData);
    connect(ui->actionComboBox, QnComboboxCurrentIndexChanged, this, &EventLogDialog::updateData);
    connect(ui->refreshButton, &QAbstractButton::clicked, this, &EventLogDialog::updateData);
    connect(ui->eventRulesButton, &QAbstractButton::clicked,
        action(menu::OpenVmsRulesDialogAction), &QAction::trigger);

    connect(ui->cameraButton, &QAbstractButton::clicked,
        this, &EventLogDialog::at_cameraButton_clicked);
    connect(ui->gridEvents, &QTableView::clicked, this, &EventLogDialog::at_eventsGrid_clicked);
    connect(ui->gridEvents, &QTableView::customContextMenuRequested,
        this, &EventLogDialog::at_eventsGrid_customContextMenuRequested);
    connect(&appContext()->localSettings()->resourceInfoLevel,
        &nx::utils::property_storage::BaseProperty::changed,
        ui->gridEvents,
        &QAbstractItemView::reset);

    // Pending is implemented on the dialog side, so no additional delay is needed.
    ui->textSearchLineEdit->setTextChangedSignalFilterMs(0);
    ui->textSearchLineEdit->lineEdit()->setPlaceholderText(Strings::description());
    connect(ui->textSearchLineEdit, &SearchLineEdit::textChanged,
        this, &EventLogDialog::updateDataDelayed);
    m_delayUpdateTimer.setSingleShot(true);
    m_delayUpdateTimer.setInterval(kUpdateDelay);
    connect(&m_delayUpdateTimer, &QTimer::timeout, this, &EventLogDialog::updateData);

    const auto updateServerEventsMenuIfNeeded =
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (resource->hasFlags(Qn::server))
                {
                    updateServerEventsMenu();
                    break;
                }
            }
        };

    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this, updateServerEventsMenuIfNeeded);
    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this, updateServerEventsMenuIfNeeded);

    ItemViewAutoHider::create(ui->gridEvents, tr("No events"));

    reset();
}

EventLogDialog::~EventLogDialog()
{}

QStandardItem* EventLogDialog::createEventTree(const Group& group)
{
    auto item = new QStandardItem(group.name);
    item->setData(QString::fromStdString(group.id), EventTypeRole);

    // TODO: #amalov Filter nvr events?

    for (const auto& eventType: group.items)
    {
        auto eventItem = new QStandardItem(Strings::eventName(systemContext(), eventType));
        eventItem->setData(eventType, EventTypeRole);
        item->appendRow(eventItem);

        if (eventType == kAnalyticsEventType)
            createAnalyticsEventTree(eventItem);
    }

    for (const auto& subGroup: group.groups)
    {
        item->appendRow(createEventTree(subGroup));
    }

    return item;
}

void EventLogDialog::createAnalyticsEventTree(QStandardItem* rootItem)
{
    using AnalyticsTreeBuilder = core::AnalyticsEntitiesTreeBuilder;
    auto addItem =
        [](QStandardItem* parent, AnalyticsTreeBuilder::NodePtr node)
        {
            auto item = new QStandardItem(node->text);
            item->setData(kAnalyticsEventType, EventTypeRole);
            item->setData(QVariant::fromValue(node->entityId), AnalyticsEventTypeIdRole);
            item->setSelectable(node->nodeType == AnalyticsTreeBuilder::NodeType::eventType);

            if (NX_ASSERT(parent))
                parent->appendRow(item);
            return item;
        };

    auto addItemRecursive = nx::utils::y_combinator(
        [addItem](auto addItemRecursive, auto parent, auto root) -> void
        {
            for (auto node: root->children)
            {
                const auto menuItem = addItem(parent, node);
                addItemRecursive(menuItem, node);
            }
            if (!root->children.empty())
                parent->sortChildren(0);
        });

    const auto root = systemContext()->analyticsEventsSearchTreeBuilder()->eventTypesTree();
    addItemRecursive(rootItem, root);
}

void EventLogDialog::updateAnalyticsEvents()
{
    const auto index = analyticsEventsRoot(m_eventTypesModel);
    if (!index.isValid())
        return;

    auto item = m_eventTypesModel->itemFromIndex(index);
    NX_ASSERT(item);
    if (!item)
        return;

    const auto selectedIndex = ui->eventComboBox->currentModelIndex();
    const auto selectedEventType = selectedIndex.data(EventTypeRole).toInt();
    const auto selectedAnalyticsEventTypeId =
        selectedEventType == nx::vms::api::EventType::analyticsSdkEvent
            ? analyticsEventTypeId(selectedIndex)
            : nx::vms::api::analytics::EventTypeId();

    item->removeRows(0, item->rowCount());
    createAnalyticsEventTree(item);

    if (!selectedAnalyticsEventTypeId.isNull())
    {
        // Only select current item.
        QSignalBlocker signalBlocker(ui->eventComboBox);
        setAnalyticsEventType(selectedAnalyticsEventTypeId);
    }

    resizeTableColumns();
}

void EventLogDialog::updateServerEventsMenu()
{
    const auto anyServerEventIndex =
        findServerEventsMenuIndexByEventType(QModelIndex(), kServerIssueEventGroup);
    if (anyServerEventIndex == QModelIndex())
        return;

    const auto anyServerEventItem = m_eventTypesModel->itemFromIndex(anyServerEventIndex);
    auto selectedEventType = eventType(ui->eventComboBox->currentModelIndex());
    const auto serverGroup =
        systemContext()->vmsRulesEngine()->eventGroups().findGroup(kServerIssueEventGroup);

    /* TODO: #amalov Filter inaccessible events?
    const auto accessibleEvents = serverGroup.items;NvrEventsActionsAccess::removeInacessibleNvrEvents(
        childEvents(EventType::anyServerEvent), resourcePool());

    anyServerEventItem->removeRows(0, anyServerEventItem->rowCount());
    for (auto childValue: accessibleEvents)
        createEventTree(anyServerEventItem, childValue);

    if (parentEvent(selectedEventType) == EventType::anyServerEvent)
    {
        if (!accessibleEvents.contains(selectedEventType))
            selectedEventType = EventType::anyServerEvent;
        ui->eventComboBox->setCurrentIndex(
            findServerEventsMenuIndexByEventType(QModelIndex(), selectedEventType));
    }
    */
}

QModelIndex EventLogDialog::findServerEventsMenuIndexByEventType(
    const QModelIndex& rootIndex,
    const QString& eventType) const
{
    for (int row = 0; row < m_eventTypesModel->rowCount(rootIndex); ++row)
    {
        const auto rowIndex = m_eventTypesModel->index(row, 0, rootIndex);
        const auto eventTypeData = rowIndex.data(EventTypeRole);
        if (eventTypeData.isNull())
            continue;

        if (eventTypeData.toString() == eventType)
            return rowIndex;

        const auto childIndex = findServerEventsMenuIndexByEventType(rowIndex, eventType);
        if (childIndex != QModelIndex())
            return childIndex;
    }
    return QModelIndex();
}

bool EventLogDialog::isFilterExist() const
{
    return ui->actionComboBox->currentIndex() > 0
        || !eventType(ui->eventComboBox->currentModelIndex()).isEmpty()
        || !resourcePool()->getResourcesByIds<QnVirtualCameraResource>(eventDevices()).empty();
}

void EventLogDialog::initEventsModel()
{
    auto rootItem = createEventTree(systemContext()->vmsRulesEngine()->eventGroups());
    m_eventTypesModel->appendRow(rootItem);
    ui->eventComboBox->setModel(m_eventTypesModel);

    auto updateAnalyticsSubmenuOperation = new nx::utils::PendingOperation(
        [this] { updateAnalyticsEvents(); },
        100ms,
        this);
    updateAnalyticsSubmenuOperation->fire();
    updateAnalyticsSubmenuOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

    connect(systemContext()->analyticsEventsSearchTreeBuilder(),
        &core::AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged,
        updateAnalyticsSubmenuOperation,
        &nx::utils::PendingOperation::requestOperation);
}

void EventLogDialog::initActionsModel()
{
    QStandardItem* anyActionItem = new QStandardItem(tr("Any Action"));
    anyActionItem->setData(QString(), ActionTypeRole);
    anyActionItem->setData(false, ProlongedOnlyActionRole);
    m_actionTypesModel->appendRow(anyActionItem);

    // TODO: #amalov Filter inaccessible actions?

    for (const auto& actionDesc: systemContext()->vmsRulesEngine()->actions())
    {
        QStandardItem* item = new QStandardItem(
            Strings::actionName(systemContext(), actionDesc.id));
        item->setData(actionDesc.id, ActionTypeRole);
        item->setData(isProlongedOnly(actionDesc), ProlongedOnlyActionRole);

        m_actionTypesModel->appendRow(item);
    }
    ui->actionComboBox->setModel(m_actionTypesModel);
    resizeTableColumns();
}

void EventLogDialog::reset()
{
    disableUpdateData();
    setEventType({});
    setEventDevices({});
    setActionType({});
    setText(QString());
    ui->dateRangeWidget->reset();
    enableUpdateData();
    setLoading(State::loaded);
}

void EventLogDialog::setLoading(
    EventLogDialog::State state, nx::network::rest::ErrorId error)
{
    const bool loading = (state == State::loading);
    setCursor(loading ? Qt::BusyCursor : Qt::ArrowCursor);

    switch (state)
    {
        case State::loading:
            ui->stackedWidget->setCurrentWidget(ui->progressPage);
            break;

        case State::loaded:
            ui->stackedWidget->setCurrentWidget(ui->gridPage);
            break;

        case State::error:
            ui->warningLabel->setText(errorText(error));
            ui->stackedWidget->setCurrentWidget(ui->warnPage);
            break;

        default:
            NX_ASSERT("Unexpected dialog state: %1", (int) state);
            break;
    }
}

QString EventLogDialog::errorText(nx::network::rest::ErrorId error)
{
    using nx::network::rest::ErrorId;

    switch (error)
    {
        case ErrorId::serviceUnavailable:
            return tr("Server cannot answer in reasonable time. Consider narrowing period or filter.");
    }

    return tr("All Servers are offline. Logs are not available.");
}

void EventLogDialog::updateDataDelayed()
{
    m_delayUpdateTimer.stop();
    m_delayUpdateTimer.start();
}

void EventLogDialog::updateData()
{
    if (m_updateDisabled)
    {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;
    m_delayUpdateTimer.stop();

    const auto index = ui->eventComboBox->currentModelIndex();
    const auto eventType = desktop::eventType(index);
    const auto eventDesc = systemContext()->vmsRulesEngine()->eventDescriptor(eventType);
    const bool serverIssue = (eventType == kServerIssueEventGroup)
        || (eventDesc && eventDesc->groupId == kServerIssueEventGroup);

    ui->cameraButton->setEnabled(!serverIssue);
    if (serverIssue)
        setEventDevices({});

    bool instantOnly = eventDesc && !eventDesc->flags.testFlag(ItemFlag::prolonged);
    if (const auto analyticsTypeId = analyticsEventTypeId(index);
        !instantOnly && !analyticsTypeId.isEmpty())
    {
        const auto taxonomy = systemContext()->analyticsTaxonomyState();
        const auto analyticsType = taxonomy->eventTypeById(analyticsTypeId);
        instantOnly = analyticsType && !analyticsType->isStateDependent();
    }

    updateActionList(instantOnly);

    const auto actionType = desktop::actionType(
        m_actionTypesModel->index(ui->actionComboBox->currentIndex(), 0));

    const auto analyticsEventTypeId = (eventType == kAnalyticsEventType)
        ? desktop::analyticsEventTypeId(ui->eventComboBox->currentModelIndex())
        : nx::vms::api::analytics::EventTypeId();

    const auto order =
        ui->gridEvents->horizontalHeader()->sortIndicatorSection() == EventLogModel::DateTimeColumn
        ? ui->gridEvents->horizontalHeader()->sortIndicatorOrder()
        : Qt::SortOrder{};

    query(
        ui->dateRangeWidget->period(),
        eventType,
        analyticsEventTypeId,
        actionType,
        ui->textSearchLineEdit->text(),
        order);

    m_resetFilterAction->setEnabled(isFilterExist());

    if (m_request)
    {
        setLoading(State::loading);
    }
    else
    {
        requestFinished(/*records*/ {},  nx::network::rest::ErrorId::badRequest);
    }

    m_updateDisabled = false;
    m_dirty = false;
}

void EventLogDialog::query(
    const QnTimePeriod& period,
    const QString& eventType,
    const nx::vms::api::analytics::EventTypeId& analyticsEventTypeId,
    const QString& actionType,
    const QString& text,
    Qt::SortOrder order)
{
    m_request = {};

    auto serverApi = connectedServerApi();
    if (!serverApi)
        return;

    nx::vms::api::rules::EventLogFilter filter(period.toServerPeriod());
    filter.eventType = typeList(eventType);
    filter.eventSubtype = analyticsEventTypeId;
    filter.actionType = typeList(actionType);
    filter.text = text;
    filter.limit = kMaxEventLogToRequest + 1;
    filter.order = order;

    const auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(eventDevices());
    if (!cameras.empty())
    {
        filter.eventResourceId = std::vector<QString>();
        for (const auto& camera: cameras)
            filter.eventResourceId->valueOrArray.push_back(camera->getId().toSimpleString());
    }

    auto callback =
        [this](
            bool success,
            rest::Handle handle,
            rest::ErrorOrData<nx::vms::api::rules::EventLogRecordList> records)
        {
            if (m_request != handle)
                return;

            if (records)
            {
                requestFinished(std::move(*records), nx::network::rest::ErrorId::ok);
            }
            else
            {
                NX_ERROR(this, "Can't read event log, success: %1, code: %2, error: %3",
                    success, records.error().errorId, records.error().errorString);

                requestFinished(/*records*/ {}, records.error().errorId);
            }

        };

    rest::ServerConnection::Timeouts timeouts;
    timeouts.responseReadTimeout = kQueryTimeout;

    m_request = serverApi->eventLog(filter, std::move(callback), this, timeouts);
}

void EventLogDialog::retranslateUi()
{
    ui->retranslateUi(this);
    auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(eventDevices());

    if (cameras.empty())
        ui->cameraButton->selectAny();
    else
        ui->cameraButton->selectDevices(cameras);

    // TODO: #amalov Update device dependent strings (event/action names).
    ui->eventRulesButton->setVisible(menu()->canTrigger(menu::OpenVmsRulesDialogAction));
}

/*
 * Resizes event log table columns:
 * - Sets fixed widths for DateTime, Action, and Event columns.
 * - Distributes remaining space evenly across other columns.
 */
void EventLogDialog::resizeTableColumns()
{
    if (!ui->gridEvents->model() || !ui->eventComboBox->model())
        return;

    const auto dateTimeWidth = dateTimeColumnDefaultWidth(ui->gridEvents);
    const auto actionColumnWidth = calculateItemMaxWidth(
        ui->gridEvents->fontMetrics(), system()->vmsRulesEngine()->actions());
    const auto eventColumnWidth =
        calculateItemMaxWidth(ui->gridEvents->fontMetrics(), system()->vmsRulesEngine()->events());

    const std::unordered_map<EventLogModel::Column, int> fixedSizeColumns = {
        {EventLogModel::DateTimeColumn, dateTimeWidth},
        {EventLogModel::ActionColumn, actionColumnWidth},
        {EventLogModel::EventColumn, eventColumnWidth}
    };

    const auto totalWidth = width() - style::Metrics::kStandardPadding * 2;
    const auto basicColumnWidth =
        (totalWidth - dateTimeWidth - actionColumnWidth - eventColumnWidth)
        / (EventLogModel::ColumnCount - fixedSizeColumns.size());

    for (int i = 0; i < EventLogModel::ColumnCount; i++)
    {
        auto it = fixedSizeColumns.find(static_cast<EventLogModel::Column>(i));
        ui->gridEvents->setColumnWidth(
            i, it == fixedSizeColumns.end() ? basicColumnWidth : it->second);
    }
}

void EventLogDialog::requestFinished(
    nx::vms::api::rules::EventLogRecordList&& records, nx::network::rest::ErrorId error)
{
    const bool hasMoreRecords = records.size() > kMaxEventLogToRequest;
    if (hasMoreRecords)
        records.erase(records.begin() + kMaxEventLogToRequest, records.end());

    m_model->setEvents(std::move(records));

    if (error != nx::network::rest::ErrorId::ok)
    {
        setLoading(State::error, error);
        return;
    }

    QLocale locale;
    auto start = ui->dateRangeWidget->startDate();
    auto end = ui->dateRangeWidget->endDate();

    if (start != end)
    {
        if (hasMoreRecords)
        {
            ui->statusLabel->setText(
                tr("Event log for period from %1 to %2 - Showing first %n events. To find "
                    "specific events, please adjust the date range or apply additional filters",
                    "Dates are substituted", m_model->rowCount())
                .arg(locale.toString(start, QLocale::LongFormat))
                .arg(locale.toString(end, QLocale::LongFormat)));
        }
        else
        {
            ui->statusLabel->setText(
                tr("Event log for period from %1 to %2 - %n events found",
                    "Dates are substituted", m_model->rowCount())
                .arg(locale.toString(start, QLocale::LongFormat))
                .arg(locale.toString(end, QLocale::LongFormat)));
        }
    }
    else // Single day filter.
    {
        if (hasMoreRecords)
        {
            ui->statusLabel->setText(
                tr("Event log for %1 - Showing first %n events. To find specific events, please "
                    "adjust the date range or apply additional filters",
                    "Date is substituted",
                    m_model->rowCount())
                .arg(locale.toString(start, QLocale::LongFormat)));
        }
        else
        {
            ui->statusLabel->setText(
                tr("Event log for %1 - %n events found", "Date is substituted", m_model->rowCount())
                .arg(locale.toString(start, QLocale::LongFormat)));
        }
    }

    setLoading(State::loaded);
}

void EventLogDialog::at_eventsGrid_clicked(const QModelIndex& idx)
{
    if (m_lastMouseButton != Qt::LeftButton)
        return;

    const auto resources = m_model->resourcesForPlayback(idx).filtered(
        [this](const QnResourcePtr& resource)
        {
            return accessController()->hasPermissions(resource, Qn::ViewContentPermission);
        });

    if (resources.isEmpty())
        return;

    const auto archiveCameras = resources.filtered<QnVirtualCameraResource>(
        [this](const auto& camera)
        {
            return accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
        });

    menu::Parameters params(resources);
    if (resources.size() == archiveCameras.size())
        params.setArgument(Qn::ItemTimeRole, m_model->eventTimestamp(idx.row()).count());

    menu()->trigger(menu::OpenInNewTabAction, params);

    if (isMaximized())
        showNormal();
}

void EventLogDialog::setEventType(const QString& value)
{
    const auto found = m_eventTypesModel->match(
        m_eventTypesModel->index(0, 0),
        EventTypeRole,
        /*value*/value,
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (found.isEmpty())
        ui->eventComboBox->setCurrentIndex(QModelIndex());
    else
        ui->eventComboBox->setCurrentIndex(found.first());
}

void EventLogDialog::setAnalyticsEventType(nx::vms::api::analytics::EventTypeId value)
{
    const QModelIndexList indices = m_eventTypesModel->match(
        m_eventTypesModel->index(0, 0),
        AnalyticsEventTypeIdRole,
        /*value*/ QVariant::fromValue(value),
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (indices.size() == 1)
        ui->eventComboBox->setCurrentIndex(indices[0]);
    else if (QModelIndex root = analyticsEventsRoot(m_eventTypesModel); root.isValid())
        ui->eventComboBox->setCurrentIndex(root);
}

void EventLogDialog::setDateRange(qint64 startTimeMs, qint64 endTimeMs)
{
    ui->dateRangeWidget->setRange(startTimeMs, endTimeMs);
}

void EventLogDialog::resetDateRange()
{
    ui->dateRangeWidget->reset();
}

const UuidSet& EventLogDialog::eventDevices() const
{
    return m_eventDevices;
}

void EventLogDialog::setEventDevices(const UuidSet& ids)
{
    if (ids == m_eventDevices)
        return;

    m_eventDevices = ids;
    retranslateUi();
    updateAnalyticsEvents();
    updateData();
}

void EventLogDialog::setActionType(const QString& value)
{
    for (int i = 0; i < m_actionTypesModel->rowCount(); ++i)
    {
        if (actionType(m_actionTypesModel->index(i, 0)) == value)
        {
            ui->actionComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void EventLogDialog::setText(const QString& text)
{
    ui->textSearchLineEdit->lineEdit()->setText(text);
}

void EventLogDialog::at_filterAction_triggered()
{
    QModelIndex idx = ui->gridEvents->currentIndex();

    const auto recordEventType = m_model->eventType(idx.row());
    const auto recordAnalyticsEventType = m_model->analyticsEventType(idx.row());
    const auto recordActionType = m_model->actionType(idx.row());

    const auto filterActionType =
        actionType(m_actionTypesModel->index(ui->actionComboBox->currentIndex(), 0));

/* TODO: #amalov Investigate parent stuff.
    auto parentEventType = parentEvent(recordEventType);
    if (parentEventType != EventType::anyEvent && parentEventType != EventType::undefinedEvent)
        recordEventType = parentEventType;
*/

    // Add device from the Source column to the filter if it is present.
    QSet<nx::Uuid> camList;
    auto eventSourceIdx = idx.sibling(idx.row(), EventLogModel::EventSourceColumn);
    const auto cameraResource = m_model->data(eventSourceIdx, core::ResourceRole)
        .value<QnResourcePtr>()
        .dynamicCast<QnVirtualCameraResource>();
    if (cameraResource)
        camList << cameraResource->getId();

    disableUpdateData();
    if (idx.column() == EventLogModel::ActionColumn)
    {
        setActionType(recordActionType);
    }
    else
    {
        if (!recordAnalyticsEventType.isEmpty())
            setAnalyticsEventType(recordAnalyticsEventType);
        else
            setEventType(recordEventType);

        setEventDevices(camList);

        if (filterActionType != recordActionType)
            setActionType({});
    }
    enableUpdateData();
}

void EventLogDialog::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QScopedPointer<QMenu> menu;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = m_model->data(idx, core::ResourceRole).value<QnResourcePtr>();
        auto manager = this->menu();
        if (resource && systemContext()->accessController()->hasPermissions(resource,
            Qn::ViewContentPermission))
        {
            menu::Parameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, ResourceTree::NodeType::resource);

            menu.reset(manager->newMenu(menu::TreeScope, this, parameters));
            for (QAction* action: menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu.reset(new QMenu(this));

    m_filterAction->setEnabled(idx.isValid());
    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addAction(m_filterAction);
    menu->addAction(m_resetFilterAction);

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void EventLogDialog::at_exportAction_triggered()
{
    QnTableExportHelper::exportToFile(
        ui->gridEvents->model(),
        ui->gridEvents->selectionModel()->selectedIndexes(),
        this,
        tr("Export selected events to file"));
}

void EventLogDialog::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(
        ui->gridEvents->model(),
        ui->gridEvents->selectionModel()->selectedIndexes());
}

void EventLogDialog::at_mouseButtonRelease(QObject* /*sender*/, QEvent* event)
{
    const auto me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void EventLogDialog::at_cameraButton_clicked()
{
    auto cameraIds = eventDevices();
    auto dialogAccepted = CameraSelectionDialog::selectCameras
        <CameraSelectionDialog::DummyPolicy>(systemContext(), cameraIds, this);

    if (dialogAccepted)
        setEventDevices(cameraIds);
}

void EventLogDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void EventLogDialog::enableUpdateData()
{
    m_updateDisabled = false;

    if (m_dirty)
    {
        m_dirty = false;
        if (isVisible())
            updateData();
    }
}

void EventLogDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_dirty = true;
    enableUpdateData();
}

void EventLogDialog::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    disableUpdateData();
}

void EventLogDialog::updateActionList(bool instantOnly)
{
    const auto prolongedActions = m_actionTypesModel->match(
        m_actionTypesModel->index(0, 0),
        ProlongedOnlyActionRole,
        /*value*/ true,
        /*hits*/ -1,
        Qt::MatchExactly);

    // what type of actions to show: prolonged or instant
    bool enableProlongedActions = !instantOnly;
    for (const auto& idx: prolongedActions)
    {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }

    if (!m_actionTypesModel->item(ui->actionComboBox->currentIndex())->isEnabled())
        ui->actionComboBox->setCurrentIndex(0);
}

} // namespace nx::vms::client::desktop
