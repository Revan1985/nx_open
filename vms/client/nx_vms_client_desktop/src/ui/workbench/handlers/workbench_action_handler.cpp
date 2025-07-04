// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_action_handler.h"

#include <QtCore/QProcess>
#include <QtCore/QScopeGuard>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtGui/QImage>
#include <QtGui/QImageWriter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWhatsThis>

#include <api/network_proxy_factory.h>
#include <api/server_rest_connection.h>
#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/desktop_client_message_processor.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <network/authutil.h>
#include <nx/analytics/utils.h>
#include <nx/build_info.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/math/math.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/platform/process.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/camera_resource.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/event_search/widgets/advanced_search_dialog.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/profile_g/import_from_device_dialog.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/network/cloud_url_validator.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_replacement_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/failover_priority_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/resource_properties/camera/dialogs/camera_credentials_dialog.h>
#include <nx/vms/client/desktop/resource_properties/layout/flux/layout_settings_dialog_state.h>
#include <nx/vms/client/desktop/resource_properties/media_file/media_file_settings_dialog.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/screen_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_administration/dialogs/integrations_dialog.h>
#include <nx/vms/client/desktop/system_administration/widgets/advanced_system_settings_widget.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/context_current_user_watcher.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/system_update/advanced_update_settings_dialog.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/ui/main_window.h>
#include <nx/vms/client/desktop/ui/messages/local_files_messages.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/ui/messages/videowall_messages.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/state/thumbnail_search_state.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/utils/camera_hotspots_support.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/rules/actions/write_to_log_action.h>
#include <nx/vms/rules/group.h>
#include <nx/vms/rules/utils/type.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <platform/environment.h>
#include <recording/time_period_list.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/dialogs/about_dialog.h>
#include <ui/dialogs/adjust_video_dialog.h>
#include <ui/dialogs/camera_diagnostics_dialog.h>
#include <ui/dialogs/camera_password_change_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/local_settings_dialog.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/dialogs/ping_dialog.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/main_window_title_bar_widget.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/applauncher_utils.h>
#include <utils/common/delayed.h>
#include <utils/common/delete_later.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/email/email.h>
#include <utils/unity_launcher_workaround.h>

#if defined(Q_OS_MACOS)
    #include <utils/mac_utils.h>
#endif

using namespace std::chrono;

using nx::vms::client::core::Geometry;
using nx::vms::client::core::MotionSelection;
using namespace nx::vms::common;

namespace {

const char* uploadingImageARPropertyName = "_qn_uploadingImageARPropertyName";

constexpr int kSectionHeight = 20;

constexpr qint64 kOneDayOffsetMs = 60 * 60 * 24 * 1000;

constexpr qint64 kOneWeekOffsetMs = kOneDayOffsetMs * 7;

} // namespace

//!time that is given to process to exit. After that, applauncher (if present) will try to terminate it
static const quint32 PROCESS_TERMINATE_TIMEOUT = 15000;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

using utils::UnityLauncherWorkaround;

// -------------------------------------------------------------------------- //
// ActionHandler
// -------------------------------------------------------------------------- //
ActionHandler::ActionHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &ActionHandler::at_context_userChanged);

    connect(workbench(), &Workbench::itemChanged, this,
        &ActionHandler::at_workbench_itemChanged);
    connect(workbench(), &Workbench::cellSpacingChanged, this,
        &ActionHandler::at_workbench_cellSpacingChanged);

    connect(action(menu::AboutAction), &QAction::triggered, this,
        &ActionHandler::at_aboutAction_triggered);

    connect(action(menu::UserManualAction), &QAction::triggered, this,
        [] { HelpHandler::openHelpTopic(HelpTopic::Id::MainWindow_Scene); },
        Qt::QueuedConnection);

    connect(action(menu::OpenFileAction), &QAction::triggered, this,
        &ActionHandler::at_openFileAction_triggered);
    connect(action(menu::OpenFolderAction), &QAction::triggered, this,
        &ActionHandler::at_openFolderAction_triggered);

    connect(globalSettings(), &SystemSettings::maxSceneItemsChanged, this,
        [this]
        {
            qnRuntime->setMaxSceneItemsOverride(globalSettings()->maxSceneItemsOverride());
        });

    // local settings
    connect(action(menu::PreferencesGeneralTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::GeneralPage); },
        Qt::QueuedConnection);

    connect(action(menu::PreferencesLookAndFeelTabAction), &QAction::triggered, this,
        [this] { openLocalSettingsDialog(QnLocalSettingsDialog::LookAndFeelPage); },
        Qt::QueuedConnection);

    // System administration.
    connect(action(menu::SystemAdministrationAction), &QAction::triggered,
        this, &ActionHandler::at_systemAdministrationAction_triggered);

    connect(action(menu::AdvancedUpdateSettingsAction), &QAction::triggered,
        this, &ActionHandler::at_advancedUpdateSettingsAction_triggered);

    // TODO: #vkutin #sivanov Remove these actions, use FocusTabRole in action parameters instead.
    connect(action(menu::PreferencesLicensesTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::LicensesPage); });
    connect(action(menu::PreferencesServicesTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::SaasInfoPage); });
    connect(action(menu::PreferencesSmtpTabAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::MailSettingsPage); });
    connect(action(menu::PreferencesCloudTabAction), &QAction::triggered, this,
        [this]{ openSystemAdministrationDialog(QnSystemAdministrationDialog::CloudManagement); });
    connect(action(menu::SystemUpdateAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UpdatesPage); });
    connect(action(menu::UserManagementAction), &QAction::triggered, this,
        [this] { openSystemAdministrationDialog(QnSystemAdministrationDialog::UserManagement); });
    connect(action(menu::LogsManagementAction), &QAction::triggered, this,
        [this]
        {
            openSystemAdministrationDialog(
                QnSystemAdministrationDialog::Advanced,
                AdvancedSystemSettingsWidget::urlFor(AdvancedSystemSettingsWidget::Subpage::logs));
        });
    connect(action(menu::AnalyticsEngineSettingsAction), &QAction::triggered, this,
        [this]
        {
            QUrl url;

            if (const nx::Uuid& engineId = menu()->currentParameters(sender()).resource()->getId();
                !engineId.isNull())
            {
                url.setQuery("engineId=" + engineId.toString(QUuid::WithBraces));
            }

            openSystemAdministrationDialog(QnSystemAdministrationDialog::Analytics, url);
        });

    connect(action(menu::OpenFailoverPriorityAction), &QAction::triggered, this,
        &ActionHandler::openFailoverPriorityDialog);
    connect(action(menu::OpenBookmarksSearchAction), &QAction::triggered, this,
        &ActionHandler::at_openBookmarksSearchAction_triggered);
    connect(action(menu::OpenIntegrationsAction), &QAction::triggered, this,
        &ActionHandler::openIntegrationsActionTriggered);
    connect(action(menu::OpenAuditLogAction), &QAction::triggered, this,
        &ActionHandler::at_openAuditLogAction_triggered);
    connect(action(menu::CameraListAction), &QAction::triggered, this
        , &ActionHandler::at_cameraListAction_triggered);
    connect(action(menu::CameraListByServerAction), &QAction::triggered, this,
        &ActionHandler::at_cameraListAction_triggered);

    connect(action(menu::WebClientAction), &QAction::triggered, this,
        &ActionHandler::at_webClientAction_triggered);
    connect(action(menu::WebAdminAction), &QAction::triggered, this,
        &ActionHandler::at_webAdminAction_triggered);

    connect(action(menu::NextLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_nextLayoutAction_triggered);
    connect(action(menu::PreviousLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_previousLayoutAction_triggered);

    connect(action(menu::OpenAction), &QAction::triggered, this,
        &ActionHandler::at_openAction_triggered);
    connect(action(menu::OpenInLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_openInLayoutAction_triggered);
    connect(action(menu::OpenInCurrentLayoutAction), &QAction::triggered, this,
        &ActionHandler::at_openInCurrentLayoutAction_triggered);

    connect(action(menu::OpenInNewWindowAction), &QAction::triggered, this,
        &ActionHandler::at_openInNewWindowAction_triggered);
    connect(action(menu::MonitorInNewWindowAction), &QAction::triggered, this,
        &ActionHandler::at_openInNewWindowAction_triggered);

    connect(action(menu::OpenCurrentLayoutInNewWindowAction), &QAction::triggered, this,
        &ActionHandler::at_openCurrentLayoutInNewWindowAction_triggered);
    connect(action(menu::OpenNewWindowAction), &QAction::triggered,
        this, &ActionHandler::at_openNewWindowAction_triggered);
    connect(action(menu::OpenWelcomeScreenAction), &QAction::triggered,
        this, &ActionHandler::at_openWelcomeScreenAction_triggered);
    connect(action(menu::ReviewShowreelInNewWindowAction), &QAction::triggered, this,
        &ActionHandler::at_reviewShowreelInNewWindowAction_triggered);

    connect(action(menu::ChangeDefaultCameraPasswordAction), &QAction::triggered,
        this, &ActionHandler::at_changeDefaultCameraPassword_triggered);

    connect(action(menu::MediaFileSettingsAction), &QAction::triggered, this,
        &ActionHandler::at_mediaFileSettingsAction_triggered);
    connect(action(menu::ReplaceCameraAction), &QAction::triggered,
        this, &ActionHandler::replaceCameraActionTriggered);
    connect(action(menu::UndoReplaceCameraAction), &QAction::triggered,
        this, &ActionHandler::undoReplaceCameraActionTriggered);
    connect(action(menu::CameraIssuesAction), &QAction::triggered, this,
        &ActionHandler::at_cameraIssuesAction_triggered);

    connect(action(menu::CameraDiagnosticsAction), &QAction::triggered, this,
        &ActionHandler::at_cameraDiagnosticsAction_triggered);
    connect(action(menu::PingAction), &QAction::triggered, this,
        &ActionHandler::at_pingAction_triggered);
    connect(action(menu::ServerLogsAction), &QAction::triggered, this,
        &ActionHandler::at_serverLogsAction_triggered);
    connect(action(menu::ServerIssuesAction), &QAction::triggered, this,
        &ActionHandler::at_serverIssuesAction_triggered);
    connect(action(menu::OpenInFolderAction), &QAction::triggered, this,
        &ActionHandler::at_openInFolderAction_triggered);
    connect(action(menu::DeleteFromDiskAction), &QAction::triggered, this,
        &ActionHandler::at_deleteFromDiskAction_triggered);
    connect(action(menu::RemoveFromServerAction), &QAction::triggered, this,
        &ActionHandler::at_removeFromServerAction_triggered);
    connect(action(menu::RenameResourceAction), &QAction::triggered, this,
        &ActionHandler::at_renameAction_triggered);
    connect(action(menu::DropResourcesAction), &QAction::triggered, this,
        &ActionHandler::at_dropResourcesAction_triggered);

    connect(action(menu::MoveCameraAction), &QAction::triggered, this,
        &ActionHandler::at_moveCameraAction_triggered);
    connect(action(menu::AdjustVideoAction), &QAction::triggered, this,
        &ActionHandler::at_adjustVideoAction_triggered);

    connect(action(menu::SaveSessionState), &QAction::triggered, this,
        &ActionHandler::at_saveSessionState_triggered);
    connect(action(menu::RestoreSessionState), &QAction::triggered, this,
        &ActionHandler::at_restoreSessionState_triggered);
    connect(action(menu::DeleteSessionState), &QAction::triggered, this,
        &ActionHandler::at_deleteSessionState_triggered);

    connect(action(menu::CloseAllWindowsAction), &QAction::triggered, this,
        [this]()
        {
            if (auto session = system()->session())
                session->autoTerminateIfNeeded();
            closeAllWindows();
        });

    connect(action(menu::ExitAction), &QAction::triggered, this,
        [this]()
        {
            if (auto session = system()->session())
                session->autoTerminateIfNeeded();
            closeApplication();
        });

    connect(
        appContext()->sharedMemoryManager(),
        &nx::vms::client::desktop::SharedMemoryManager::clientCommandRequested,
        this,
        &ActionHandler::at_clientCommandRequested);
    connect(action(menu::ThumbnailsSearchAction), &QAction::triggered, this,
        &ActionHandler::at_thumbnailsSearchAction_triggered);
    connect(action(menu::CreateZoomWindowAction), &QAction::triggered, this,
        &ActionHandler::at_createZoomWindowAction_triggered);

    connect(action(menu::SetCurrentLayoutItemSpacingNoneAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(core::CellSpacing::None);
        });

    connect(action(menu::SetCurrentLayoutItemSpacingSmallAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(core::CellSpacing::Small);
        });

    connect(action(menu::SetCurrentLayoutItemSpacingMediumAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(core::CellSpacing::Medium);
        });

    connect(action(menu::SetCurrentLayoutItemSpacingLargeAction), &QAction::triggered, this,
        [this]
        {
            setCurrentLayoutCellSpacing(core::CellSpacing::Large);
        });

    connect(action(menu::RotateToAction), &QAction::triggered, this,
        [this]
        {
            const auto params = menu()->currentParameters(sender());
            const auto rotation = params.argument(Qn::ItemRotationRole, 0.0);
            rotateItems(rotation);
        });

    connect(action(menu::SetAsBackgroundAction), &QAction::triggered, this,
        &ActionHandler::at_setAsBackgroundAction_triggered);
    connect(action(menu::WhatsThisAction), &QAction::triggered, this,
        &ActionHandler::at_whatsThisAction_triggered);
    connect(action(menu::EscapeHotkeyAction), &QAction::triggered, this,
        &ActionHandler::at_escapeHotkeyAction_triggered);
    connect(action(menu::BrowseUrlAction), &QAction::triggered, this,
        &ActionHandler::at_browseUrlAction_triggered);
    connect(action(menu::BetaVersionMessageAction), &QAction::triggered, this,
        &ActionHandler::at_betaVersionMessageAction_triggered);
    connect(action(menu::ConfirmAnalyticsStorageAction), &QAction::triggered, this,
        &ActionHandler::confirmAnalyticsStorageLocation);

    connect(action(menu::QueueAppRestartAction), &QAction::triggered, this,
        &ActionHandler::at_queueAppRestartAction_triggered);
    connect(action(menu::SelectTimeServerAction), &QAction::triggered, this,
        &ActionHandler::at_selectTimeServerAction_triggered);

    connect(action(menu::BetaUpgradeWarningAction), &QAction::triggered, this,
        &ActionHandler::handleBetaUpdateWarning);

    connect(action(menu::JumpToTimeAction), &QAction::triggered,
        this, &ActionHandler::at_jumpToTimeAction_triggered);

    connect(action(menu::GoToLayoutItemAction), &QAction::triggered,
        this, &ActionHandler::at_goToLayoutItemAction_triggered);

    connect(action(menu::DelayedForcedExitAction), &QAction::triggered, this,
        &ActionHandler::closeApplication, Qt::QueuedConnection);

    connect(action(menu::BeforeExitAction), &QAction::triggered, this,
        &ActionHandler::at_beforeExitAction_triggered);

    connect(action(menu::OpenNewSceneAction), &QAction::triggered, this,
        &ActionHandler::at_openNewScene_triggered);

    connect(action(menu::OpenAdvancedSearchDialog), &QAction::triggered,
        this, &ActionHandler::at_openAdvancedSearchDialog_triggered);

    connect(action(menu::OpenImportFromDevices), &QAction::triggered,
        this, &ActionHandler::at_openImportFromDevicesDialog_triggered);

    connect(action(menu::ReplaceLayoutItemAction), &QAction::triggered,
        this, &ActionHandler::replaceLayoutItemActionTriggered);

    connect(action(menu::ItemMuteAction), &QAction::triggered,
        this, &ActionHandler::at_itemMuteAction_triggered);

    connect(action(menu::ItemUnmuteAction), &QAction::triggered,
        this, &ActionHandler::at_itemUnmuteAction_triggered);

    connect(action(menu::CameraAuthenticationAction), &QAction::triggered,
        this, &ActionHandler::cameraAuthenticationActionTriggered);
}

ActionHandler::~ActionHandler()
{
    deleteDialogs();
}

void ActionHandler::addToLayout(
    const core::LayoutResourcePtr& layout,
    const QnResourcePtr& resource,
    const AddToLayoutParams& params)
{
    if (!layout)
        return;

    if (!ResourceAccessManager::hasPermissions(layout, Qn::ModifyLayoutPermissions))
    {
        m_accessDeniedMessage = SceneBanner::show(tr("Not enough access rights"));
        return;
    }

    if (qnRuntime->lightMode().testFlag(Qn::LightModeSingleItem))
    {
        while (!layout->getItems().isEmpty())
            layout->removeItem(*(layout->getItems().begin()));
    }

    int layoutSize = layout->getItems().size();
    auto tierLimit = systemContext()->saasServiceManager()->tierLimit(
        nx::vms::api::SaasTierLimitName::maxDevicesPerLayout);
    if (layoutSize >= qnRuntime->maxSceneItems() || (tierLimit.has_value() && layoutSize >= *tierLimit))
    {
        if (workbench()->currentLayout()->resource() == layout)
        {
            if (!m_layoutIsFullMessage)
                m_layoutIsFullMessage = SceneBanner::show(tr("Layout is full"));
        }
        return;
    }

    if (!menu()->canTrigger(menu::OpenInLayoutAction, menu::Parameters(resource)
        .withArgument(core::LayoutResourceRole, layout)))
    {
        return;
    }

    // Force cloud resource descriptor for cloud layouts.
    LayoutItemData data = layoutItemFromResource(resource,
        /*forceCloud*/ layout->hasFlags(Qn::cross_system));

    data.flags = params.size.has_value() && params.position.has_value() && !params.size->isEmpty()
        ? Qn::Pinned
        : Qn::PendingGeometryAdjustment;

    data.zoomRect = params.zoomWindow;
    data.zoomTargetUuid = params.zoomUuid;

    data.displayRoi = params.displayRoi;
    data.displayAnalyticsObjects = params.displayAnalyticsObjects;
    data.displayHotspots =
        params.displayHotspots
        && camera_hotspots::supportsCameraHotspots(resource)
        && resource.dynamicCast<QnVirtualCameraResource>()->cameraHotspotsEnabled();

    if (!qFuzzyIsNull(params.rotation))
    {
        data.rotation = params.rotation;
    }
    data.contrastParams = params.contrastParams;
    data.dewarpingParams = params.dewarpingParams;

    if (params.timelineWindow.isValid())
    {
        layout->setItemData(
            data.uuid,
            Qn::ItemSliderWindowRole,
            QVariant::fromValue(params.timelineWindow));
    }

    if (params.timelineSelection.isValid())
    {
        layout->setItemData(
            data.uuid,
            Qn::ItemSliderSelectionRole,
            QVariant::fromValue(params.timelineSelection));
    }

    if (!params.motionSelection.empty())
    {
        layout->setItemData(
            data.uuid,
            Qn::ItemMotionSelectionRole,
            QVariant::fromValue(params.motionSelection));
    }

    if (params.analyticsSelection.isValid())
        layout->setItemData(data.uuid, Qn::ItemAnalyticsSelectionRole, params.analyticsSelection);

    if (params.frameDistinctionColor.isValid())
    {
        layout->setItemData(
            data.uuid,
            Qn::ItemFrameDistinctionColorRole,
            params.frameDistinctionColor);
    }

    if (!params.zoomWindowRectangleVisible)
        layout->setItemData(data.uuid, Qn::ItemZoomWindowRectangleVisibleRole, false);

    if (params.position)
    {
        // Top left of a null size rect will be used as desired position, geometry will be
        // adjusted to grid, valid rect will be set as is.
        const auto size = params.size.value_or(QSizeF());
        data.combinedGeometry = QRectF(params.position.value(), size);
        data.combinedGeometry.translate(-Geometry::toPoint(size) / 2.0);
    }
    else
    {
        // Negative size rect means that any suitable geometry may be applied.
        data.combinedGeometry = QRectF(QPointF(0.5, 0.5), QPointF(-0.5, -0.5));
    }

    if (params.zoomed)
        layout->setItemData(data.uuid, Qn::ItemAddInZoomedStateRole, true);

    layout->addItem(data);

    // Cleanup "zoomed" state to ensure further switches to this item will not be affected.
    layout->setItemData(data.uuid, Qn::ItemAddInZoomedStateRole, QVariant());

    if (resource.dynamicCast<QnAviResource>() && layoutSize == 0)
    {
        // Rewind local files to the beginning on an empty layout.
        layout->setItemData(data.uuid, Qn::ItemTimeRole, 0);
        layout->setItemData(data.uuid, Qn::ItemPausedRole, false);
        layout->setItemData(data.uuid, Qn::ItemSpeedRole, 1);
    }
    else if (params.time >= 0ms)
    {
        layout->setItemData(data.uuid, Qn::ItemTimeRole, (qint64) params.time.count());
        layout->setItemData(data.uuid, Qn::ItemPausedRole, params.paused);
        layout->setItemData(data.uuid, Qn::ItemSpeedRole, params.speed);
    }
}

void ActionHandler::addToLayout(
    const core::LayoutResourcePtr& layout,
    const QnResourceList& resources,
    const AddToLayoutParams& params)
{
    for (const QnResourcePtr& resource: resources)
        addToLayout(layout, resource, params);
}

void ActionHandler::addToLayout(
    const core::LayoutResourcePtr& layout,
    const QList<QnMediaResourcePtr>& resources,
    const AddToLayoutParams& params)
{
    for (const QnMediaResourcePtr& resource: resources)
        addToLayout(layout, resource, params);
}

void ActionHandler::addToLayout(
    const core::LayoutResourcePtr& layout,
    const QList<QString>& files,
    const AddToLayoutParams& params)
{
    addToLayout(layout, addToResourcePool(files), params);
}

QnResourceList ActionHandler::addToResourcePool(const QList<QString>& files) const
{
    return QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(files),
        resourcePool());
}

QnResourceList ActionHandler::addToResourcePool(const QString& file) const
{
    return QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(file),
        resourcePool());
}

void ActionHandler::openResourcesInNewWindow(const QnResourceList &resources)
{
    if (resources.isEmpty())
        return;

    MimeData data;
    data.setResources(resources);

    std::optional<core::LogonData> logonData;
    if (auto connection = this->connection())
        logonData = connection->createLogonData();

    appContext()->clientStateHandler()->createNewWindow(
        logonData,
        {data.serialized()});
}

void ActionHandler::rotateItems(int degrees) {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if (!widgets.empty()) {
        foreach(const QnResourceWidget *widget, widgets)
            widget->item()->setRotation(degrees);
    }
}

void ActionHandler::setCurrentLayoutCellSpacing(core::CellSpacing spacing)
{
    // TODO: #sivanov move out these actions to separate CurrentLayoutHandler.
    // Then at_workbench_cellSpacingChanged will also use this method.
    auto actionId = [spacing]
        {
            switch (spacing)
            {
                case core::CellSpacing::None:
                    return menu::SetCurrentLayoutItemSpacingNoneAction;
                case core::CellSpacing::Small:
                    return menu::SetCurrentLayoutItemSpacingSmallAction;
                case core::CellSpacing::Medium:
                    return menu::SetCurrentLayoutItemSpacingMediumAction;
                case core::CellSpacing::Large:
                    return menu::SetCurrentLayoutItemSpacingLargeAction;
            }
            NX_ASSERT(false);
            return menu::SetCurrentLayoutItemSpacingSmallAction;
        };

    if (auto layout = workbench()->currentLayoutResource(); NX_ASSERT(layout))
    {
        layout->setPredefinedCellSpacing(spacing);
        action(actionId())->setChecked(true);
    }
}

QnCameraListDialog *ActionHandler::cameraListDialog() const {
    return m_cameraListDialog.data();
}

QnSystemAdministrationDialog *ActionHandler::systemAdministrationDialog() const {
    return m_systemAdministrationDialog.data();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void ActionHandler::at_context_userChanged(const QnUserResourcePtr& user)
{
    m_serverRequests.clear();
    if (!user && m_advancedSearchDialog)
        m_advancedSearchDialog->deleteLater();
}

void ActionHandler::at_workbench_cellSpacingChanged()
{
    qreal value = workbench()->currentLayout()->cellSpacing();

    if (core::LayoutResource::isEqualCellSpacing(core::CellSpacing::None, value))
        action(menu::SetCurrentLayoutItemSpacingNoneAction)->setChecked(true);
    else if (core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Small, value))
        action(menu::SetCurrentLayoutItemSpacingSmallAction)->setChecked(true);
    else if (core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Medium, value))
        action(menu::SetCurrentLayoutItemSpacingMediumAction)->setChecked(true);
    else if (core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Large, value))
        action(menu::SetCurrentLayoutItemSpacingLargeAction)->setChecked(true);
    else
        action(menu::CurrentLayoutItemSpacingCustomMenuItem)->setChecked(true);
}

void ActionHandler::at_nextLayoutAction_triggered()
{
    if (action(menu::ToggleShowreelModeAction)->isChecked())
        return;

    const auto total = workbench()->layouts().size();
    if (total == 0)
        return;

    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() + 1) % total);
}

void ActionHandler::at_previousLayoutAction_triggered()
{
    if (action(menu::ToggleShowreelModeAction)->isChecked())
        return;

    const auto total = workbench()->layouts().size();
    if (total == 0)
        return;

    workbench()->setCurrentLayoutIndex((workbench()->currentLayoutIndex() - 1 + total) % total);
}

void ActionHandler::showSingleCameraErrorMessage(const QString& explanation)
{
    static const auto kMessageText = tr("Failed to change password");
    QnMessageBox::critical(mainWindowWidget(), kMessageText, explanation);
}

void ActionHandler::showMultipleCamerasErrorMessage(
    int totalCameras,
    const QnVirtualCameraResourceList& camerasWithError,
    const QString& explanation)
{
    static const auto kSimpleOptions = QnResourceListView::Options(
        QnResourceListView::HideStatusOption
        | QnResourceListView::ServerAsHealthMonitorOption
        | QnResourceListView::SortAsInTreeOption);

    const QString message = tr("Failed to change password on %n cameras of %1",
        "Total number of cameras will be substituted as %1",
        camerasWithError.size()).arg(totalCameras);

    QnSessionAwareMessageBox messageBox(mainWindowWidget());
    messageBox.setIcon(QnMessageBoxIcon::Critical);
    messageBox.setText(message);
    if (!explanation.isEmpty())
        messageBox.setInformativeText(explanation);
    messageBox.addCustomWidget(
        new QnResourceListView(camerasWithError, kSimpleOptions, &messageBox),
        QnMessageBox::Layout::AfterMainLabel);

    messageBox.setStandardButtons(QDialogButtonBox::Ok);
    messageBox.exec();
}

void ActionHandler::changeDefaultPasswords(
    const QString& previousPassword,
    const QnVirtualCameraResourceList& cameras,
    bool forceShowCamerasList)
{
    if (!NX_ASSERT(connection()))
        return;

    auto dialog = createSelfDestructingDialog<QnCameraPasswordChangeDialog>(
        previousPassword, cameras, forceShowCamerasList, mainWindowWidget());

    connect(
        dialog,
        &QnCameraPasswordChangeDialog::accepted,
        this,
        [this, dialog, cameras, forceShowCamerasList]
        {
            onDefaultPasswordChanged(dialog->password(), cameras, forceShowCamerasList);
        });

    dialog->show();
}

void ActionHandler::onDefaultPasswordChanged(
    const QString& password,
    const QnVirtualCameraResourceList& cameras,
    bool forceShowCamerasList)
{
    using PasswordChangeResult = QPair<QnVirtualCameraResourcePtr, nx::network::rest::Result>;
    using PasswordChangeResultList = QList<PasswordChangeResult>;

    const auto errorResultsStorage = QSharedPointer<PasswordChangeResultList>(
        new PasswordChangeResultList());

    const auto completionGuard = nx::utils::makeSharedGuard(nx::utils::guarded(this,
        [this, cameras, errorResultsStorage, password, forceShowCamerasList]()
        {
            if (errorResultsStorage->isEmpty())
                return;

            // Show error dialog and try one more time
            const auto camerasWithError =
                [errorResultsStorage]()
                {
                    QnVirtualCameraResourceList result;
                    for (const auto& errorResult: *errorResultsStorage)
                        result.push_back(errorResult.first);
                    return result;
                }();

            auto explanation =
                [errorResultsStorage]()
                {
                    nx::network::rest::ErrorId error = errorResultsStorage->first().second.errorId;
                    if (errorResultsStorage->size() != 1)
                    {
                        const bool hasAnotherError = std::any_of(
                            errorResultsStorage->begin() + 1, errorResultsStorage->end(),
                            [error](const PasswordChangeResult& result)
                            {
                                return error != result.second.errorId;
                            });

                        if (hasAnotherError)
                            error = nx::network::rest::ErrorId::ok;
                    }

                    return error == nx::network::rest::ErrorId::ok
                        ? QString() //< NoError means different or unspecified error
                        : errorResultsStorage->first().second.errorString;
                }();

            if (cameras.size() > 1)
                showMultipleCamerasErrorMessage(cameras.size(), camerasWithError, explanation);
            else
                showSingleCameraErrorMessage(explanation);

            changeDefaultPasswords(password, camerasWithError, forceShowCamerasList);
        }));

    for (const auto& camera: cameras)
    {
        auto auth = camera->getDefaultAuth();
        auth.setPassword(password);

        const auto resultCallback =
            [completionGuard, camera, errorResultsStorage]
                (bool, rest::Handle, rest::ErrorOrEmpty reply)
            {
                if (reply)
                    return;

                errorResultsStorage->append(
                    PasswordChangeResult(camera, reply.error()));
            };

        connectedServerApi()->changeCameraPassword(camera, auth,
            nx::utils::guarded(this, resultCallback), QThread::currentThread());
    }
}

void ActionHandler::at_changeDefaultCameraPassword_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto camerasWithDefaultPassword =
        parameters.resources().filtered<QnVirtualCameraResource>();

    if (camerasWithDefaultPassword.isEmpty())
    {
        NX_ASSERT(false, "No cameras with default password");
        return;
    }

    const bool forceShowCamerasList = parameters.argument(Qn::ForceShowCamerasList, false);
    changeDefaultPasswords(QString(), camerasWithDefaultPassword, forceShowCamerasList);
}

void ActionHandler::at_openAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());
    QnResourceList resources = parameters.resources();

    // Remove web pages as they are processed in QnWorkbenchWebPageHandler.
    resources.removeIf(
        [](const QnResourcePtr& resource) { return resource->hasFlags(Qn::web_page); });

    parameters.setResources(resources);
    menu()->trigger(menu::OpenInCurrentLayoutAction, parameters);
}

void ActionHandler::at_openInLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    core::LayoutResourcePtr layout = parameters.argument<core::LayoutResourcePtr>(core::LayoutResourceRole);
    if (!NX_ASSERT(layout))
        return;

    QnResourceList resources = parameters.resources();
    if (!layout->hasFlags(Qn::cross_system)
        && std::any_of(resources.cbegin(), resources.cend(),
            [](const QnResourcePtr& resource) { return resource->hasFlags(Qn::cross_system); }))
    {
        NX_ASSERT(parameters.widgets().empty());

        // Convert common layout to cloud one.
        auto cloudLayout = appContext()->cloudLayoutsManager()->convertLocalLayout(layout);
        // Replace opened layout with the cloud one.
        auto targetLayout = workbench()->replaceLayout(layout, cloudLayout);
        if (!targetLayout)
            targetLayout = workbench()->addLayout(cloudLayout);

        if (NX_ASSERT(targetLayout))
            workbench()->setCurrentLayout(targetLayout);

        menu()->trigger(menu::OpenInCurrentLayoutAction, parameters);

        return;
    }

    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

    const int maxItems = qnRuntime->maxSceneItems();

    bool adjustAspectRatio = (layout->getItems().isEmpty() || !layout->hasCellAspectRatio())
        && !layout->hasBackground();

    QnResourceWidgetList widgets = parameters.widgets();
    if (!widgets.empty() && position.isNull() && layout->getItems().empty())
    {
        static const auto kExtraItemRoles =
            {Qn::ItemTimeRole, Qn::ItemPausedRole, Qn::ItemSpeedRole};

        using ExtraItemDataHash = QHash<Qn::ItemDataRole, QVariant>;
        QHash<nx::Uuid, ExtraItemDataHash> extraItemRoleValues;

        QHash<nx::Uuid, LayoutItemData> itemDataByUuid;
        for (auto widget: widgets)
        {
            LayoutItemData data = widget->item()->data();
            const auto oldUuid = data.uuid;
            data.uuid = nx::Uuid::createUuid();
            data.flags = Qn::PendingGeometryAdjustment;
            itemDataByUuid[oldUuid] = data;

            // Do not save position and state for cameras which footage we cannot see.
            if (const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>())
            {
                if (!ResourceAccessManager::hasPermissions(camera, Qn::ViewFootagePermission))
                    continue;
            }

            for (const auto extraItemRole: kExtraItemRoles)
            {
                const QVariant value = layout->itemData(oldUuid, extraItemRole);
                if (value.isValid())
                    extraItemRoleValues[data.uuid].insert(extraItemRole, value);
            }
        }

        /* Update cross-references. */
        for (auto pos = itemDataByUuid.begin(); pos != itemDataByUuid.end(); pos++)
        {
            if (!pos->zoomTargetUuid.isNull())
                pos->zoomTargetUuid = itemDataByUuid[pos->zoomTargetUuid].uuid;
        }

        /* Add to layout. */
        for (const auto& data: itemDataByUuid)
        {
            if (layout->getItems().size() >= maxItems)
                return;

            layout->addItem(data);

            const auto values = extraItemRoleValues[data.uuid];
            for (auto it = values.begin(); it != values.end(); ++it)
                layout->setItemData(data.uuid, it.key(), it.value());
        }
    }
    else if (!resources.isEmpty())
    {
        AddToLayoutParams addParams;
        if (!position.isNull())
            addParams.position = position;
        addParams.timelineWindow =
            parameters.argument<QnTimePeriod>(Qn::ItemSliderWindowRole);
        addParams.timelineSelection =
            parameters.argument<QnTimePeriod>(Qn::ItemSliderSelectionRole);
        addParams.motionSelection =
            parameters.argument<MotionSelection>(Qn::ItemMotionSelectionRole);
        addParams.analyticsSelection =
            parameters.argument<QRectF>(Qn::ItemAnalyticsSelectionRole);
        addParams.zoomed =
            parameters.argument<bool>(Qn::ItemAddInZoomedStateRole, false);

        addParams.time = std::chrono::milliseconds(DATETIME_NOW);
        addParams.paused = false;
        addParams.speed = 1.0;

        const bool canViewFootage = std::any_of(resources.begin(), resources.end(),
            [](auto resource)
            {
                return ResourceAccessManager::hasPermissions(resource,
                    Qn::Permission::ViewFootagePermission);
            });

        if (canViewFootage) //< Live viewers must not open items at archive position.
        {
            if (parameters.hasArgument(Qn::ItemTimeRole))
            {
                addParams.time = milliseconds(parameters.argument<qint64>(Qn::ItemTimeRole));
                addParams.paused = parameters.argument<bool>(Qn::ItemPausedRole, false);
                addParams.speed = parameters.argument<qreal>(Qn::ItemSpeedRole, 1.0);
            }
            else if (parameters.hasArgument(Qn::LayoutSyncStateRole))
            {
                const bool canViewLive = std::any_of(resources.begin(), resources.end(),
                    [](auto resource)
                    {
                        return ResourceAccessManager::hasPermissions(resource,
                            Qn::Permission::ViewLivePermission);
                    });

                const auto state =
                    parameters.argument<StreamSynchronizationState>(Qn::LayoutSyncStateRole);
                if ((!state.isSyncOn || state.timeUs < 0) && !canViewLive)
                {
                    addParams.time = 0ms;
                }
                else
                {
                    addParams.time = milliseconds(
                        (state.timeUs == DATETIME_NOW || state.timeUs < 0)
                            ? DATETIME_NOW
                            : (state.timeUs / 1000));
                }
                addParams.paused = qFuzzyIsNull(state.speed);
                addParams.speed = state.speed;
            }
        }
        else if (parameters.hasArgument(Qn::LayoutSyncStateRole))
        {
            // When playback synchronization is on, items must be opened at the synchronized
            // playback position even if there are no permissions to view archive.
            const auto state =
                parameters.argument<StreamSynchronizationState>(Qn::LayoutSyncStateRole);
            addParams.time = milliseconds(
                (state.timeUs == DATETIME_NOW || state.timeUs < 0)
                    ? DATETIME_NOW
                    : (state.timeUs / 1000));
        }

        addToLayout(layout, resources, addParams);
    }

    auto workbenchLayout = workbench()->currentLayout();
    if (adjustAspectRatio && workbenchLayout->resource() == layout)
    {
        qreal midAspectRatio = 0.0;
        int count = 0;

        if (!widgets.isEmpty())
        {
            /* Here we don't take into account already added widgets. It's ok because
            we can get here only if the layout doesn't have cell aspect ratio, that means
            its widgets don't have aspect ratio too. */

            for (auto widget: widgets)
            {
                if (widget->hasAspectRatio())
                {
                    midAspectRatio += widget->visualAspectRatio();
                    ++count;
                }
            }
        }
        else
        {
            for (auto item: workbenchLayout->items())
            {
                QnResourceWidget *widget = context()->display()->widget(item);
                if (!widget)
                    continue;

                float aspectRatio = widget->visualChannelAspectRatio();
                if (aspectRatio <= 0)
                    continue;

                midAspectRatio += aspectRatio;
                ++count;
            }
        }

        if (count > 0)
        {
            midAspectRatio /= count;
            QnAspectRatio cellAspectRatio = QnAspectRatio::closestStandardRatio(midAspectRatio);
            layout->setCellAspectRatio(cellAspectRatio.toFloat());
        }
        else if (workbenchLayout->items().size() > 1)
        {
            layout->setCellAspectRatio(QnLayoutResource::kDefaultCellAspectRatio);
        }
    }
}

void ActionHandler::replaceLayoutItemActionTriggered()
{
    using namespace std::chrono;

    const auto actionParams = menu()->currentParameters(sender());

    const auto layoutItemIndex = menu::ParameterTypes::layoutItem(actionParams.widget());
    if (layoutItemIndex.isNull())
        return;

    const auto camera = actionParams.argument(
        core::ResourceRole).value<QnVirtualCameraResourcePtr>();
    if (!camera)
        return;

    const auto layout = layoutItemIndex.layout();
    const auto replacedItemData = layout->getItem(layoutItemIndex.uuid());

    AddToLayoutParams replacementParams;
    replacementParams.position = replacedItemData.combinedGeometry.center();
    replacementParams.size = replacedItemData.combinedGeometry.size();

    if (actionParams.hasArgument(Qn::ItemTimeRole))
        replacementParams.time = milliseconds(actionParams.argument<qint64>(Qn::ItemTimeRole));

    if (actionParams.hasArgument(Qn::ItemSpeedRole))
        replacementParams.speed = actionParams.argument<qreal>(Qn::ItemSpeedRole);

    if (actionParams.hasArgument(Qn::ItemPausedRole))
        replacementParams.paused = actionParams.argument<bool>(Qn::ItemPausedRole);

    replacementParams.zoomed = actionParams.argument<bool>(Qn::ItemAddInZoomedStateRole, false);

    const auto display = context()->display();
    const auto forceNoAnimationGuard = qScopeGuard(
        [display, val = display->forceNoAnimation()] { display->setForceNoAnimation(val); });
    context()->display()->setForceNoAnimation(true);

    layout->removeItem(layoutItemIndex.uuid());
    addToLayout(layout, camera, replacementParams);
}

void ActionHandler::cameraAuthenticationActionTriggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto cameras = parameters.resources().filtered<QnVirtualCameraResource>(
        [](const auto& camera)
        {
            return !camera->hasFlags(Qn::cross_system) && !camera->hasFlags(Qn::removed);
        });

    if (cameras.empty())
        return;

    std::optional<QString> displayedLogin = cameras.first()->getAuth().user();
    const auto multipleLoginValues = std::any_of(cameras.begin(), cameras.end(),
        [login = displayedLogin.value()](const auto& camera)
        {
            return camera->getAuth().user() != login;
        });

    if (multipleLoginValues)
        displayedLogin.reset();

    auto dialog = createSelfDestructingDialog<CameraCredentialsDialog>(mainWindowWidget());
    dialog->setLogin(displayedLogin);
    dialog->setHasRemotePassword(true);

    connect(
        dialog,
        &CameraCredentialsDialog::accepted,
        this,
        [dialog, cameras]
        {
            for (const auto& camera: cameras)
            {
                QAuthenticator auth;
                auth.setUser(dialog->login().value_or(camera->getAuth().user()));
                auth.setPassword(dialog->password().value());

                if (camera->isMultiSensorCamera() || camera->isNvr())
                {
                    nx::vms::client::core::CameraResource::setAuthToCameraGroup(camera, auth);
                }
                else
                {
                    camera->setAuth(auth);
                    camera->savePropertiesAsync();
                }
            }
        });

    dialog->show();
}

void ActionHandler::at_openInCurrentLayoutAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());
    const auto currentLayout = workbench()->currentLayout();

    // Check if we are in videowall control mode
    nx::Uuid videoWallItemGuid = currentLayout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
    if (!videoWallItemGuid.isNull())
    {
        QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(videoWallItemGuid);
        const auto resources = parameters.resources();

        // Displaying message delayed to avoid waiting cursor (see drop_instrument.cpp:245)
        if (!messages::Videowall::checkLocalFiles(mainWindowWidget(), index, resources, true))
            return;
    }

    nx::utils::ScopedConnection connection;
    if (parameters.argument<bool>(Qn::SelectOnOpeningRole))
    {
        connection.reset(connect(workbench()->currentLayout(), &QnWorkbenchLayout::itemAdded, this,
            [this, &connection](const QnWorkbenchItem* item)
            {
                menu()->trigger(menu::GoToLayoutItemAction, menu::Parameters()
                    .withArgument(Qn::ItemUuidRole, item->uuid()));

                connection.reset();
            }));
    }
    if (!parameters.hasArgument(Qn::LayoutSyncStateRole)
        && !parameters.hasArgument(Qn::ItemTimeRole))
    {
        parameters.setArgument(Qn::LayoutSyncStateRole,
            workbench()->windowContext()->streamSynchronizer()->state());
    }

    parameters.setArgument(core::LayoutResourceRole, currentLayout->resource());
    menu()->trigger(menu::OpenInLayoutAction, parameters);
}

void ActionHandler::at_openInNewWindowAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnResourceList filtered;
    for (const auto& resource: parameters.resources())
    {
        if (menu()->canTrigger(menu::OpenInNewWindowAction, resource))
            filtered << resource;
    }
    if (filtered.isEmpty())
        return;

    openResourcesInNewWindow(filtered);
}

void ActionHandler::at_openCurrentLayoutInNewWindowAction_triggered()
{
    menu()->trigger(menu::OpenInNewWindowAction, workbench()->currentLayout()->resource());
}

void ActionHandler::at_openNewWindowAction_triggered()
{
    if (ini().enableMultiSystemTabBar)
    {
        appContext()->clientStateHandler()->createNewWindow(
            /*logonData*/ std::nullopt,
            /*args*/ {QnStartupParameters::kSkipAutoLoginKey});
        return;
    }

    std::optional<core::LogonData> logonData;
    if (auto connection = this->connection())
        logonData = connection->createLogonData();
    appContext()->clientStateHandler()->createNewWindow(logonData);
}

void ActionHandler::at_openWelcomeScreenAction_triggered()
{
    if (ini().enableMultiSystemTabBar)
    {
        action(menu::ResourcesModeAction)->setChecked(false);
    }
    else
    {
        appContext()->clientStateHandler()->createNewWindow(
            /*logonData*/ std::nullopt,
            /*args*/ {QnStartupParameters::kSkipAutoLoginKey});
    }
}

void ActionHandler::at_reviewShowreelInNewWindowAction_triggered()
{
    auto connection = this->connection();
    if (!NX_ASSERT(connection, "Client must be logged in"))
        return;

    // For now place method here until openNewWindow code would be shared.
    const auto parameters = menu()->currentParameters(sender());
    auto id = parameters.argument<nx::Uuid>(core::UuidRole);

    MimeData data;
    data.setEntities({id});
    appContext()->clientStateHandler()->createNewWindow(
        connection->createLogonData(),
        {data.serialized()});
}

std::optional<QnVirtualCameraResourceList> ActionHandler::checkCameraList(
    bool success,
    int handle,
    const QnCameraListReply& reply)
{
    if (!m_awaitingMoveCameras.contains(handle))
        return std::nullopt;
    QnVirtualCameraResourceList modifiedResources = m_awaitingMoveCameras.value(handle).cameras;
    QnMediaServerResourcePtr server = m_awaitingMoveCameras.value(handle).dstServer;
    m_awaitingMoveCameras.remove(handle);

    if (!success)
    {
        const auto text = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Failed to move %n devices", "", modifiedResources.size()),
                tr("Failed to move %n cameras", "", modifiedResources.size()),
                tr("Failed to move %n I/O Modules", "", modifiedResources.size())),
            modifiedResources);

        const auto extras = tr("Server \"%1\" is not responding.").arg(server->getName());
        QnMessageBox messageBox(QnMessageBoxIcon::Critical,
            text, extras, QDialogButtonBox::Ok, QDialogButtonBox::Ok,
            mainWindowWidget());

        messageBox.addCustomWidget(new QnResourceListView(modifiedResources, &messageBox));
        messageBox.exec();
        return std::nullopt;
    }

    QnVirtualCameraResourceList errorResources; // TODO: #sivanov Check server cameras.
    for (auto itr = modifiedResources.begin(); itr != modifiedResources.end();)
    {
        if (!reply.physicalIdList.contains((*itr)->getPhysicalId()))
        {
            errorResources << *itr;
            itr = modifiedResources.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    if (!errorResources.empty())
    {
        const auto text = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Server \"%1\" cannot access %n devices. Move them anyway?",
                    "", errorResources.size()),
                tr("Server \"%1\" cannot access %n cameras. Move them anyway?",
                    "", errorResources.size()),
                tr("Server \"%1\" cannot access %n I/O modules. Move them anyway?",
                    "", errorResources.size())),
            errorResources).arg(server->getName());

        QnMessageBox messageBox(QnMessageBoxIcon::Warning,
            text, QString(),
            QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
            mainWindowWidget());

        messageBox.addButton(tr("Move"), QDialogButtonBox::YesRole, Qn::ButtonAccent::Standard);
        const auto skipButton = messageBox.addCustomButton(QnMessageBoxCustomButton::Skip,
            QDialogButtonBox::NoRole);
        messageBox.addCustomWidget(new QnResourceListView(errorResources, &messageBox));

        const auto result = messageBox.exec();
        if (result == QDialogButtonBox::Cancel)
            return std::nullopt;

        /* If user is sure, return invalid cameras back to list. */
        if (skipButton != messageBox.clickedButton())
            modifiedResources << errorResources;
    }

    const nx::Uuid serverId = server->getId();
    qnResourcesChangesManager->saveCameras(modifiedResources,
        [serverId](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setPreferredServerId(serverId);
        });

    qnResourcesChangesManager->saveCamerasCore(modifiedResources,
        [serverId](const QnVirtualCameraResourcePtr &camera)
        {
            camera->setParentId(serverId);
        });

    return modifiedResources;
}

void ActionHandler::at_openNewScene_triggered()
{
    if (!m_mainWindow)
    {
        m_mainWindow = std::make_unique<experimental::MainWindow>(
            appContext()->qmlEngine(),
            windowContext());
        m_mainWindow->resize(mainWindowWidget()->size());
    }

    m_mainWindow->show();
    m_mainWindow->raise();
}

void ActionHandler::moveResourcesToServer(
    const QnResourceList& resources,
    const QnMediaServerResourcePtr& server,
    MoveResourcesResultFunc resultCallback)
{
    if (!NX_ASSERT(connection()))
        return;

    const auto webPages = resources.filtered<QnWebPageResource>();

    // Only admin can move webpages.
    if (webPages.count() > 0 && !accessController()->hasPowerUserPermissions())
    {
        resultCallback({});
        return;
    }

    if (!messages::Resources::moveProxiedWebPages(mainWindowWidget(), webPages, server))
    {
        resultCallback({});
        return;
    }

    // We can safely move webpages - the user is admin or there are no webpages.
    // But we can do it only after ensuring that cameras are also moved. Otherwise we end up with
    // inconsistent state - resources split between two servers.
    const auto serverId = server ? server->getId() : nx::Uuid();
    const auto moveWebPages =
        [serverId](const QnWebPageResourceList& webPages)
        {
            for (const auto& webPage: webPages)
            {
                webPage->setProxyId(serverId);
                qnResourcesChangesManager->saveWebPage(webPage);
            }
        };

    QnVirtualCameraResourceList camerasToMove;

    if (server) //< Null server is for webpages only, cameras cannot be moved out of the server.
    {
        camerasToMove = resources.filtered<QnVirtualCameraResource>(
            [serverId = server->getId()](const auto& camera)
            {
                // Moving camera into its owner does nothing, also skip cameras from other systems.
                return camera->getParentId() != serverId
                    && !camera->flags().testFlag(Qn::cross_system);
            });

        QnVirtualCameraResourceList nonMovableCameras;

        const auto isVirtual =
            [](const QnVirtualCameraResourcePtr& camera)
            {
                return camera->hasFlags(Qn::virtual_camera);
            };

        const auto isUsb =
            [](const QnVirtualCameraResourcePtr& camera)
            {
                // TODO: #sivanov Vendor constants defined in server module are unreachable.
                static constexpr auto kUsbCamVendor = "usb_cam";
                static constexpr auto kRaspberryCamVendor = "Raspberry Pi";

                const auto cameraVendor = camera->getVendor();
                return cameraVendor == kUsbCamVendor || cameraVendor == kRaspberryCamVendor;
            };

        bool hasVirtualCameras = false;
        bool hasUsbCameras = false;

        std::copy_if(
            camerasToMove.cbegin(),
            camerasToMove.cend(),
            std::back_inserter(nonMovableCameras),
            [&isVirtual, &isUsb, &hasVirtualCameras, &hasUsbCameras](
                const QnVirtualCameraResourcePtr& camera)
            {
                const bool cameraIsVirtual = isVirtual(camera);
                const bool cameraIsUsb = isUsb(camera);

                hasVirtualCameras |= cameraIsVirtual;
                hasUsbCameras |= cameraIsUsb;

                return cameraIsVirtual
                    || cameraIsUsb
                    || camera->hasCameraCapabilities(
                        nx::vms::api::DeviceCapability::boundToServer);
            });

        // Operation is not performed if resource list contains USB or virtual cameras.
        if (!nonMovableCameras.empty())
        {
            if (nonMovableCameras.size() < camerasToMove.size())
            {
                // Some cameras from the group cannot be moved,
                // ask the user if we should move the rest anyways.

                const auto targetServerName = server->getName();
                const auto currentServerName =
                    nonMovableCameras.first()->getParentResource()->getName();

                const bool moveAnyways =
                    messages::Resources::moveCamerasToServer(
                        mainWindowWidget(),
                        nonMovableCameras,
                        targetServerName,
                        currentServerName,
                        hasVirtualCameras,
                        hasUsbCameras);

                if (moveAnyways)
                {
                    const auto toRemove = nx::utils::toQSet(nonMovableCameras);
                    const auto end = std::remove_if(camerasToMove.begin(), camerasToMove.end(),
                        [&toRemove](const QnVirtualCameraResourcePtr& camera)
                        {
                            return toRemove.contains(camera);
                        });

                    if (NX_ASSERT(end != camerasToMove.end()))
                        camerasToMove.erase(end, camerasToMove.end());
                }
                else
                {
                    resultCallback({});
                    return;
                }
            }
            else
            {
                // Cameras cannot be moved, just show a warning and abort.

                messages::Resources::warnCamerasCannotBeMoved(
                    mainWindowWidget(),
                    hasVirtualCameras,
                    hasUsbCameras);

                resultCallback({});
                return;
            }
        }
    }

    if (nx::vms::common::saas::saasInitialized(systemContext()))
    {
        const auto serviceManager = systemContext()->saasServiceManager();
        if (auto limitLeft = serviceManager->camerasTierLimitLeft(server->getId()))
        {
            if (limitLeft.value() < camerasToMove.size())
            {
                messages::Resources::warnCamerasCannotBeMovedDueTierLimit(mainWindowWidget());
                return;
            }

        }
    }

    if (!camerasToMove.isEmpty())
    {
        auto callback = nx::utils::guarded(this,
            [this, resultCallback, webPages, moveWebPages](
                bool success,
                int handle,
                const QnCameraListReply& reply)
            {
                const auto movedCameras = checkCameraList(success, handle, reply);

                QnSharedResourcePointerList<QnResource> successfullyMovedResources;

                // If movedCameras is nullopt then the whole process was unsuccessful.

                if (movedCameras)
                {
                    moveWebPages(webPages);

                    // Gather the list of successfully moved resources.
                    std::copy(
                        webPages.cbegin(),
                        webPages.cend(),
                        std::back_inserter(successfullyMovedResources));
                    std::copy(
                        movedCameras->cbegin(),
                        movedCameras->cend(),
                        std::back_inserter(successfullyMovedResources));
                }

                resultCallback(successfullyMovedResources);
            });
        int handle = connectedServerApi()->checkCameraList(
            server->getId(),
            camerasToMove,
            callback,
            thread());
        m_awaitingMoveCameras.insert(handle, CameraMovingInfo(camerasToMove, server));
    }
    else
    {
        moveWebPages(webPages);
        resultCallback(webPages);
    }
}

void ActionHandler::at_moveCameraAction_triggered() {
    const auto parameters = menu()->currentParameters(sender());

    QnResourceList resources = parameters.resources();
    QnMediaServerResourcePtr server =
        parameters.argument<QnMediaServerResourcePtr>(core::MediaServerResourceRole);

    if (!server)
        return;

    moveResourcesToServer(
        resources,
        server,
        [](QnSharedResourcePointerList<QnResource> /*moved*/){});
}

void ActionHandler::at_dropResourcesAction_triggered()
{
    const auto currentLayout = workbench()->currentLayout();
    // Showreel Handler will process this action itself.
    if (currentLayout->isShowreelReviewLayout())
        return;

    // This method can be called only from the GUI thread.
    // But it can be called from an event processed in some secondary event loop inside.
    // Therefore we have to guard it against re-entrance and set up a sort of queue.
    m_queuedDropParameters.push_back(menu()->currentParameters(sender()));
    if (m_inDropResourcesAction)
        return;

    const QScopedValueRollback guard(m_inDropResourcesAction, true);
    do
    {
        auto parameters = m_queuedDropParameters.takeFirst();

        QnResourceList resources = parameters.resources();
        QnLayoutResourceList layouts = resources.filtered<QnLayoutResource>();

        foreach (QnLayoutResourcePtr r, layouts)
            resources.removeOne(r);

        QnVideoWallResourceList videowalls = resources.filtered<QnVideoWallResource>();
        foreach (QnVideoWallResourcePtr r, videowalls)
            resources.removeOne(r);

        if (currentLayout->resource()->layoutType() == core::LayoutResource::LayoutType::videoWall)
        {
            workbenchContext()->instance<QnWorkbenchVideoWallHandler>()->
                filterAllowedMediaResources(resources);
        }

        if (!currentLayout->resource())
            menu()->trigger(menu::OpenNewTabAction);

        NX_ASSERT(currentLayout->resource());

        if (currentLayout->resource() &&
            currentLayout->resource()->locked() &&
            !resources.empty() &&
            layouts.empty() &&
            videowalls.empty())
        {
            SceneBanner::show(tr("Layout is locked and cannot be changed"));
            continue;
        }

        int totalCameras = 0;
        int viewableCameras = 0;

        for (const auto& resource: resources)
        {
            if (!resource.objectCast<QnVirtualCameraResource>())
                continue;

            ++totalCameras;

            if (ResourceAccessManager::hasPermissions(resource, Qn::ViewContentPermission))
                ++viewableCameras;
        }

        if (viewableCameras < totalCameras)
        {
            const auto warningMessageText = totalCameras == 1
                ? tr("You do not have permissions to open this camera on the layout")
                : tr("You do not have permissions to open some of selected cameras on the layout");

            SceneBanner::show(warningMessageText);
        }

        if (!resources.empty())
        {
            if (parameters.widgets().isEmpty()) //< Triggered by resources tree view
                parameters.setResources(resources);
            if (!menu()->triggerIfPossible(menu::OpenInCurrentLayoutAction, parameters))
                menu()->triggerIfPossible(menu::OpenInNewTabAction, parameters);
        }

        if (!layouts.empty())
            menu()->trigger(menu::OpenInNewTabAction, layouts);

        for (const auto& videoWall: videowalls)
            menu()->trigger(menu::OpenVideoWallReviewAction, videoWall);

    } while (!m_queuedDropParameters.empty());
}

void ActionHandler::at_openFileAction_triggered()
{
    static const QStringList kProprietaryFormats{"nov"};
    static const QStringList kVideoFormats = QnCustomFileDialog::kVideoFilter.second;
    static const QStringList kPicturesFormats = QnCustomFileDialog::picturesFilter().second;
    static const QStringList kAllSupportedFormats = kProprietaryFormats
        + kVideoFormats
        + kPicturesFormats;

    QStringList files = QFileDialog::getOpenFileNames(mainWindowWidget(),
        tr("Open File"),
        QString(),
        QnCustomFileDialog::createFilter({
            {tr("All Supported"), kAllSupportedFormats},
            QnCustomFileDialog::kVideoFilter,
            QnCustomFileDialog::picturesFilter(),
            QnCustomFileDialog::kAllFilesFilter
            }),
        nullptr,
        QnCustomFileDialog::fileDialogOptions());

    if (!files.isEmpty())
        menu()->trigger(menu::DropResourcesAction, addToResourcePool(files));
}

void ActionHandler::at_openFolderAction_triggered() {
    QString dirName = QFileDialog::getExistingDirectory(mainWindowWidget(),
        tr("Select Folder..."),
        QString(),
        QnCustomFileDialog::directoryDialogOptions());

    if (!dirName.isEmpty())
        menu()->trigger(menu::DropResourcesAction, addToResourcePool(dirName));
}

void ActionHandler::openFailoverPriorityDialog() {
    createSelfDestructingDialog<FailoverPriorityDialog>(mainWindowWidget())->show();
}

void ActionHandler::at_systemAdministrationAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const int page = parameters.hasArgument(Qn::FocusTabRole)
        ? parameters.argument(Qn::FocusTabRole).toInt()
        : QnSystemAdministrationDialog::GeneralPage;

    openSystemAdministrationDialog(page);
}

void ActionHandler::at_advancedUpdateSettingsAction_triggered()
{
    if (!m_advancedUpdateSettingsDialog)
        m_advancedUpdateSettingsDialog = new AdvancedUpdateSettingsDialog(mainWindowWidget());

    const auto parameters = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());
    m_advancedUpdateSettingsDialog->setTransientParent(parent);

    m_advancedUpdateSettingsDialog->advancedMode =
        menu()->currentParameters(sender()).argument(Qn::AdvancedModeRole).toBool();
    m_advancedUpdateSettingsDialog->open();
}

void ActionHandler::at_jumpToTimeAction_triggered()
{
    auto slider = navigator()->timeSlider();
    if (!slider || !navigator()->isTimelineRelevant())
        return;

    const auto parameters = menu()->currentParameters(sender());
    if (!parameters.hasArgument(core::TimestampRole))
    {
        NX_ASSERT(false, "Timestamp must be specified");
        return;
    }

    using namespace std::chrono;

    const auto value = parameters.argument(core::TimestampRole);
    if (!value.canConvert<microseconds>())
    {
        NX_ASSERT(false, "Unsupported timestamp value type");
        return;
    }

    microseconds timestamp = value.value<microseconds>();;

    if (timestamp < slider->minimum() || timestamp >= slider->maximum())
        return;

    // Pause playing for precise seek.
    const QnScopedTypedPropertyRollback<bool, QnWorkbenchNavigator> playingRollback(navigator(),
        &QnWorkbenchNavigator::setPlaying,
        &QnWorkbenchNavigator::isPlaying,
        false);

    const QnScopedTypedPropertyRollback<bool, QnTimeSlider> downRollback(slider,
        &QnTimeSlider::setSliderDown,
        &QnTimeSlider::isSliderDown,
        true);

    slider->navigateTo(duration_cast<milliseconds>(timestamp));
}

void ActionHandler::at_goToLayoutItemAction_triggered()
{
    const auto currentLayout = workbench()->currentLayout();
    if (!currentLayout)
        return;

    const auto parameters = menu()->currentParameters(sender());
    const auto uuid = parameters.argument(Qn::ItemUuidRole).value<nx::Uuid>();

    const auto centralItem = workbench()->item(Qn::CentralRole);

    QnWorkbenchItem* targetItem = nullptr;

    if (uuid.isNull())
    {
        const auto resource = parameters.resource();
        if (!resource)
        {
            NX_ASSERT(false, "Either item id or resource must be specified");
            return;
        }

        if (centralItem && centralItem->resource() == resource)
        {
            targetItem = centralItem;
        }
        else
        {
            const auto items = currentLayout->items(resource);
            for (const auto& item: items)
            {
                if (item->zoomRect().isValid())
                    continue; //< Skip zoom windows.

                targetItem = item;
                break;
            }
        }
    }
    else
    {
        targetItem = currentLayout->item(uuid);
    }

    if (!targetItem)
        return;

    if (parameters.argument<bool>(Qn::ItemAddInZoomedStateRole, false))
        workbench()->setItem(Qn::ZoomedRole, targetItem);

    workbench()->setItem(Qn::SingleSelectedRole, targetItem);
}

void ActionHandler::at_itemMuteAction_triggered()
{
    muteUnmuteWidgets(menu()->currentParameters(sender()).widgets(), true);
}

void ActionHandler::at_itemUnmuteAction_triggered()
{
    muteUnmuteWidgets(menu()->currentParameters(sender()).widgets(), false);
}

void ActionHandler::muteUnmuteWidgets(const QnResourceWidgetList& widgets, bool muted) const
{
    for (QnResourceWidget* widget: widgets)
    {
        auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (mediaWidget && mediaWidget->canBeMuted())
            mediaWidget->setMuted(muted);
    }
}

void ActionHandler::openSystemAdministrationDialog(int page, const QUrl& url)
{
    QnNonModalDialogConstructor<QnSystemAdministrationDialog> dialogConstructor(
        m_systemAdministrationDialog, mainWindowWidget());

    systemAdministrationDialog()->setCurrentPage(page, url);
}

void ActionHandler::openLocalSettingsDialog(int page)
{
    auto dialog = createSelfDestructingDialog<QnLocalSettingsDialog>(mainWindowWidget());
    dialog->setCurrentPage(page);
    dialog->show();
}

void ActionHandler::at_aboutAction_triggered() {
    createSelfDestructingDialog<QnAboutDialog>(mainWindowWidget())->show();
}

void ActionHandler::at_webClientAction_triggered()
{
    const auto server = currentServer();
    if (!server)
        return;

#ifdef WEB_CLIENT_SUPPORTS_PROXY
#error Reimplement VMS-6586
    openInBrowser(server, kPath, kFragment);
#endif

    if (isCloudServer(server))
    {
        menu()->trigger(menu::OpenCloudViewSystemUrl);
    }
    else
    {
        static const auto kPath = lit("/static/index.html");
        static const auto kFragment = lit("/view");
        openInBrowserDirectly(server, kPath, kFragment);
    }
}

void ActionHandler::at_webAdminAction_triggered()
{
    const auto server = menu()->currentParameters(sender()).resource()
        .dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

#ifdef WEB_CLIENT_SUPPORTS_PROXY
    openInBrowser(server);
#else
    openInBrowserDirectly(server);
#endif
}

void ActionHandler::renameLocalFile(const QnResourcePtr& resource, const QString& newName)
{
    auto newFileName = newName;
    if (nx::build_info::isWindows())
    {
        while (newFileName.endsWith(QChar(L' ')) || newFileName.endsWith(QChar(L'.')))
            newFileName.resize(newFileName.size() - 1);

        const QSet<QString> kReservedFilenames{
            lit("CON"), lit("PRN"), lit("AUX"), lit("NUL"),
            lit("COM1"), lit("COM2"), lit("COM3"), lit("COM4"),
            lit("COM5"), lit("COM6"), lit("COM7"), lit("COM8"), lit("COM9"),
            lit("LPT1"), lit("LPT2"), lit("LPT3"), lit("LPT4"), lit("LPT5"),
            lit("LPT6"), lit("LPT7"), lit("LPT8"), lit("LPT9") };

        const auto upperCaseName = newFileName.toUpper();
        if (kReservedFilenames.contains(upperCaseName))
        {
            messages::LocalFiles::reservedFilename(mainWindowWidget(), upperCaseName);
            return;
        }
    }

    static const QString kForbiddenChars =
        nx::build_info::isWindows() ? lit("\\|/:*?\"<>")
      : nx::build_info::isLinux()   ? lit("/")
      : nx::build_info::isMacOsX()  ? lit("/:")
      : QString();

    for (const auto character: newFileName)
    {
        if (kForbiddenChars.contains(character))
        {
            messages::LocalFiles::invalidChars(mainWindowWidget(), kForbiddenChars);
            return;
        }
    }

    QFileInfo fi(resource->getUrl());
    QString filePath = fi.absolutePath() + QDir::separator() + newFileName;

    if (QFileInfo::exists(filePath))
    {
        messages::LocalFiles::fileExists(mainWindowWidget(), filePath);
        return;
    }

    QString targetPath = QFileInfo(filePath).absolutePath();
    if (!QDir().mkpath(targetPath))
    {
        messages::LocalFiles::pathInvalid(mainWindowWidget(), targetPath);
        return;
    }

    QFile writabilityTest(filePath);
    if (!writabilityTest.open(QIODevice::WriteOnly))
    {
        messages::LocalFiles::fileCannotBeWritten(mainWindowWidget(), filePath);
        return;
    }

    writabilityTest.remove();

    // If video is opened right now, it is quite hard to close it synchronously.
    static const int kTimeToCloseResourceMs = 300;

    resourcePool()->removeResource(resource);
    executeDelayedParented(
        [this, resource, filePath]()
        {
            QnResourcePtr result = resource;
            const auto oldPath = resource->getUrl();

            if (!QFile::rename(oldPath, filePath))
            {
                messages::LocalFiles::fileIsBusy(mainWindowWidget(), oldPath);
            }
            // Renamed file is discovered immediately by ResourceDirectoryBrowser.
            // It will deal with adding a resource with new name
        },
        kTimeToCloseResourceMs,
        this);
}

void ActionHandler::at_openBookmarksSearchAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    // If time window is specified then set it
    const auto nowMs = qnSyncTime->currentMSecsSinceEpoch();

    const auto filterText = (parameters.hasArgument(core::BookmarkTagRole)
        ? parameters.argument(core::BookmarkTagRole).toString() : QString());

    const auto endTimeMs = nowMs;
    const auto startTimeMs = nowMs - kOneWeekOffsetMs;

    const auto dialogCreationFunction = [this, startTimeMs, endTimeMs, filterText]()
    {
        return new QnSearchBookmarksDialog(filterText, startTimeMs, endTimeMs, mainWindowWidget());
    };

    const bool firstTime = m_searchBookmarksDialog.isNull();
    const QnNonModalDialogConstructor<QnSearchBookmarksDialog> creator(m_searchBookmarksDialog, dialogCreationFunction);

    if (!firstTime)
        m_searchBookmarksDialog->setParameters(startTimeMs, endTimeMs, filterText);
}

void ActionHandler::openIntegrationsActionTriggered()
{
    if (!m_integrationsDialog)
        m_integrationsDialog = std::make_unique<IntegrationsDialog>(mainWindowWidget());

    const auto parameters = menu()->currentParameters(sender());
    const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());
    m_integrationsDialog->setTransientParent(parent);

    if (parameters.hasArgument(Qn::FocusTabRole))
    {
        m_integrationsDialog->setCurrentTab(
            parameters.argument(Qn::FocusTabRole).value<IntegrationsDialog::Tab>());
    }
    m_integrationsDialog->open();
    m_integrationsDialog->raise();
}

void ActionHandler::at_openAuditLogAction_triggered()
{
    QnNonModalDialogConstructor<QnAuditLogDialog> dialogConstructor(
        m_auditLogDialog,
        mainWindowWidget());

    const auto parameters = menu()->currentParameters(sender());

    if (parameters.hasArgument(Qn::TextRole))
    {
        const auto text = parameters.argument<QString>(Qn::TextRole);
        m_auditLogDialog->setSearchText(text);
    }

    if (parameters.hasArgument(Qn::TimePeriodRole))
    {
        const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
        m_auditLogDialog->setSearchPeriod(period);
    }

    if (parameters.hasArgument(Qn::FocusTabRole))
    {
        const auto tabIndex =
            parameters.argument<int>(Qn::FocusTabRole);
        m_auditLogDialog->setFocusTab((QnAuditLogDialog::MasterGridTabIndex) tabIndex);
    }
}

void ActionHandler::at_cameraListAction_triggered() {
    QnNonModalDialogConstructor<QnCameraListDialog> dialogConstructor(m_cameraListDialog, mainWindowWidget());
    const auto parameters = menu()->currentParameters(sender());
    QnMediaServerResourcePtr server;
    if (!parameters.resources().isEmpty())
        server = parameters.resource().dynamicCast<QnMediaServerResource>();
    cameraListDialog()->setServer(server);
}

void ActionHandler::at_thumbnailsSearchAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnResourcePtr resource = parameters.resource();
    if (!resource)
        return;

    auto widget = parameters.widget();
    if (!widget)
        return;

    bool isPreviewSearchLayout = workbench()->currentLayout()->isPreviewSearchLayout();

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);

    NX_DEBUG(this, "Start preview search, period: %1, periods: %2", period, periods);

    if (period.isEmpty())
    {
        if (!isPreviewSearchLayout)
            return;

        period = widget->item()->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();
        if (period.isEmpty())
            return;

        periods = widget->item()->data(Qn::TimePeriodsRole).value<QnTimePeriodList>();
    }

    /* Adjust for chunks. If they are provided, they MUST intersect with period */
    if (!periods.empty())
    {
        QnTimePeriodList localPeriods = periods.intersected(period);
        NX_ASSERT(!localPeriods.empty());

        if (!localPeriods.empty())
        {
            // Check if user selected period before the first chunk.
            const qint64 startDelta = localPeriods.begin()->startTimeMs - period.startTimeMs;
            if (startDelta > 0)
            {
                period.startTimeMs += startDelta;
                period.durationMs -= startDelta;
            }

            // Check if user selected period after the last chunk.
            const qint64 endDelta = period.endTimeMs() - localPeriods.last().endTimeMs();
            if (endDelta > 0)
            {
                period.durationMs -= endDelta;
            }
        }
    }

    NX_DEBUG(this, "Adjusted period: %1", period);

    /* List of possible time steps, in milliseconds. */
    static const qint64 steps[] = {
        1000ll * 10,                    /* 10 seconds. */
        1000ll * 15,                    /* 15 seconds. */
        1000ll * 20,                    /* 20 seconds. */
        1000ll * 30,                    /* 30 seconds. */
        1000ll * 60,                    /* 1 minute. */
        1000ll * 60 * 2,                /* 2 minutes. */
        1000ll * 60 * 3,                /* 3 minutes. */
        1000ll * 60 * 5,                /* 5 minutes. */
        1000ll * 60 * 10,               /* 10 minutes. */
        1000ll * 60 * 15,               /* 15 minutes. */
        1000ll * 60 * 20,               /* 20 minutes. */
        1000ll * 60 * 30,               /* 30 minutes. */
        1000ll * 60 * 60,               /* 1 hour. */
        1000ll * 60 * 60 * 2,           /* 2 hours. */
        1000ll * 60 * 60 * 3,           /* 3 hours. */
        1000ll * 60 * 60 * 6,           /* 6 hours. */
        1000ll * 60 * 60 * 12,          /* 12 hours. */
        1000ll * 60 * 60 * 24,          /* 1 day. */
        1000ll * 60 * 60 * 24 * 2,      /* 2 days. */
        1000ll * 60 * 60 * 24 * 3,      /* 3 days. */
        1000ll * 60 * 60 * 24 * 5,      /* 5 days. */
        1000ll * 60 * 60 * 24 * 10,     /* 10 days. */
        1000ll * 60 * 60 * 24 * 20,     /* 20 days. */
        1000ll * 60 * 60 * 24 * 30,     /* 30 days. */
        0,
    };

    const qint64 maxItems = appContext()->localSettings()->maxPreviewSearchItems();

    if (period.durationMs < steps[1])
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Too short period selected"),
            tr("Cannot perform Preview Search. Please select a period of 15 seconds or longer."));
        return;
    }

    /* Find best time step. */
    qint64 step = 0;
    for (int i = 0; steps[i] > 0; i++) {
        if (period.durationMs < steps[i] * (maxItems - 2)) { /* -2 is here as we're going to snap period ends to closest step points. */
            step = steps[i];
            break;
        }
    }

    int itemCount = 0;
    if (step == 0) {
        /* No luck? Calculate time step based on the maximal number of items. */
        itemCount = maxItems;

        step = period.durationMs / itemCount;
    }
    else {
        /* In this case we want to adjust the period. */

        if (resource->flags() & Qn::utc) {
            QDateTime startDateTime = QDateTime::fromMSecsSinceEpoch(period.startTimeMs);
            QDateTime endDateTime = QDateTime::fromMSecsSinceEpoch(period.endTimeMs());
            const qint64 dayMSecs = 1000ll * 60 * 60 * 24;

            if (step < dayMSecs) {
                int startMSecs = qFloor(startDateTime.date().startOfDay().msecsTo(startDateTime), step);
                int endMSecs = qCeil(endDateTime.date().startOfDay().msecsTo(endDateTime), step);

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime = startDateTime.addMSecs(startMSecs);

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime = endDateTime.addMSecs(endMSecs);
            }
            else {
                int stepDays = step / dayMSecs;

                startDateTime.setTime(QTime(0, 0, 0, 0));
                startDateTime.setDate(QDate::fromJulianDay(qFloor(startDateTime.date().toJulianDay(), stepDays)));

                endDateTime.setTime(QTime(0, 0, 0, 0));
                endDateTime.setDate(QDate::fromJulianDay(qCeil(endDateTime.date().toJulianDay(), stepDays)));
            }

            period = QnTimePeriod(startDateTime.toMSecsSinceEpoch(), endDateTime.toMSecsSinceEpoch() - startDateTime.toMSecsSinceEpoch());
        }
        else {
            qint64 startTime = qFloor(period.startTimeMs, step);
            qint64 endTime = qCeil(period.endTimeMs(), step);
            period = QnTimePeriod(startTime, endTime - startTime);
        }

        itemCount = qMin(period.durationMs / step, maxItems);
    }

    NX_DEBUG(this, "Final period: %1, step: %2 s, item count: %3", period, step / 1000.0f, itemCount);

    /* Calculate size of the resulting matrix. */
    qreal desiredItemAspectRatio = QnLayoutResource::kDefaultCellAspectRatio;
    if (widget->hasAspectRatio())
        desiredItemAspectRatio = widget->visualAspectRatio();

    /* Calculate best size for layout cells. */
    qreal desiredCellAspectRatio = desiredItemAspectRatio;

    /* Aspect ratio of the screen free space. */
    QRectF viewportGeometry = display()->boundedViewportGeometry();

    qreal displayAspectRatio = viewportGeometry.isNull()
        ? desiredItemAspectRatio
        : Geometry::aspectRatio(viewportGeometry);

    const int matrixWidth = qMax(1, qRound(std::sqrt(displayAspectRatio * itemCount / desiredCellAspectRatio)));

    /* Construct and add a new layout. */
    core::LayoutResourcePtr layout(new core::LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->setName(tr("Preview Search for %1").arg(resource->getName()));

    qint64 time = period.startTimeMs;
    for (int i = 0; i < itemCount; i++) {
        QnTimePeriod localPeriod(time, step);
        QnTimePeriodList localPeriods = periods.intersected(localPeriod);
        qint64 localTime = time;
        if (!localPeriods.empty())
            localTime = qMax(localTime, localPeriods.begin()->startTimeMs);

        LayoutItemData item = layoutItemFromResource(resource);
        item.flags = Qn::Pinned;
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.contrastParams = widget->item()->imageEnhancement();
        item.dewarpingParams = widget->item()->dewarpingParams();
        item.rotation = widget->item()->rotation();
        layout->setItemData(item.uuid, Qn::ItemPausedRole, true);
        layout->setItemData(
            item.uuid,
            Qn::ItemSliderSelectionRole,
            QVariant::fromValue(localPeriod));
        layout->setItemData(item.uuid, Qn::ItemSliderWindowRole, QVariant::fromValue(period));
        layout->setItemData(item.uuid, Qn::ItemTimeRole, localTime);
        // Set aspect ratio to make thumbnails load in all cases.
        layout->setItemData(item.uuid, Qn::ItemAspectRatioRole, desiredItemAspectRatio);
        layout->setItemData(item.uuid, Qn::TimePeriodsRole, QVariant::fromValue(localPeriods));

        layout->addItem(item);

        time += step;
    }

    layout->setData(Qn::LayoutTimeLabelsRole, true);
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutSearchStateRole,
        QVariant::fromValue<ThumbnailsSearchState>(ThumbnailsSearchState{period, step}));
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredCellAspectRatio);
    layout->setCellAspectRatio(desiredCellAspectRatio);
    layout->setLocalRange(period);

    menu()->trigger(menu::OpenInNewTabAction, menu::Parameters(layout)
        .withArgument(Qn::LayoutSyncStateRole,
            QVariant::fromValue<StreamSynchronizationState>(StreamSynchronizationState::disabled()))
    );
}

void ActionHandler::at_mediaFileSettingsAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (!resource)
        return;

    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;

    MediaFileSettingsDialog* dialog{};
    if (resource->hasFlags(Qn::remote))
    {
        dialog = createSelfDestructingDialog<QnSessionAware<MediaFileSettingsDialog>>(
            mainWindowWidget());
    }
    else
    {
        dialog = createSelfDestructingDialog<MediaFileSettingsDialog>(mainWindowWidget());
    }

    connect(
        dialog,
        &MediaFileSettingsDialog::finished,
        this,
        [this, dialog, media](int result)
        {
            if (result == QnDialog::Accepted)
            {
                dialog->submitToResource(media);
            }
            else
            {
                auto centralWidget = display()->widget(Qn::CentralRole);
                if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
                    mediaWidget->setDewarpingParams(media->getDewarpingParams());
            }
        });

    dialog->updateFromResource(media);
    dialog->show();
}

void ActionHandler::replaceCameraActionTriggered()
{
    const auto cameraToBeReplaced =
        menu()->currentParameters(sender()).resource().dynamicCast<QnVirtualCameraResource>();

    if (!NX_ASSERT(cameraToBeReplaced, "Expected parameter is missing"))
        return;

    CameraReplacementDialog replaceCameraDialog(
        cameraToBeReplaced,
        mainWindowWidget());

    const auto parentServerName = cameraToBeReplaced->getParentServer()->getName();

    if (replaceCameraDialog.isEmpty())
    {
        QnMessageBox::warning(
            mainWindowWidget(),
            tr("There are no suitable cameras for replacement on the Server \"%1\"")
                .arg(parentServerName));
        return;
    }

    replaceCameraDialog.exec();
}

void ActionHandler::undoReplaceCameraActionTriggered()
{
    using namespace rest;

    const auto camera =
        menu()->currentParameters(sender()).resource().dynamicCast<QnVirtualCameraResource>();

    if (!NX_ASSERT(camera, "Expected parameter is missing"))
        return;

    const auto pressedButton = QnMessageBox::question(
        mainWindowWidget(),
        tr("Confirm undo replacement?"),
        /*extraText*/ {},
        QDialogButtonBox::Yes | QDialogButtonBox::Cancel,
        QDialogButtonBox::Yes);

    if (pressedButton == QDialogButtonBox::Cancel)
        return;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, auto /*requestId*/, auto /*requestResult*/)
        {
            if (success)
            {
                QnMessageBox::success(
                    mainWindowWidget(),
                    tr("Undo replacement completed successfully!"),
                    tr("Some settings may not be transferred from the replaced camera due to "
                        "compatibility issues."));
            }
        });

    connectedServerApi()->undoReplaceDevice(
            camera->getId(),
            callback,
            thread());
}

void ActionHandler::at_cameraIssuesAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::EventTypeRole, QString(nx::vms::rules::kDeviceIssueEventGroup));
    parameters.setArgument(
        Qn::ActionTypeRole, vms::rules::utils::type<vms::rules::WriteToLogAction>());

    const auto now = qnSyncTime->value();
    // Set current day as time period.
    parameters.setArgument(Qn::TimePeriodRole, QnTimePeriod::fromInterval(now, now));

    menu()->trigger(menu::OpenEventLogAction, parameters);
}

void ActionHandler::at_cameraDiagnosticsAction_triggered() {
    QnVirtualCameraResourcePtr resource =
        menu()->currentParameters(sender()).resource().dynamicCast<QnVirtualCameraResource>();
    if (!resource)
        return;

    auto dialog = createSelfDestructingDialog<QnCameraDiagnosticsDialog>(mainWindowWidget());
    dialog->setResource(resource);
    dialog->restart();
    dialog->show();
}

void ActionHandler::at_serverLogsAction_triggered()
{
    QnMediaServerResourcePtr server = menu()->currentParameters(sender()).resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    openInBrowser(server, lit("/api/showLog?lines=1000"));
}

void ActionHandler::at_serverIssuesAction_triggered()
{
    auto parameters = menu()->currentParameters(sender());
    parameters.setArgument(Qn::EventTypeRole, QString(nx::vms::rules::kServerIssueEventGroup));
    parameters.setArgument(
        Qn::ActionTypeRole, vms::rules::utils::type<vms::rules::WriteToLogAction>());

    const auto now = qnSyncTime->value();
    // Set current day as time period.
    parameters.setArgument(Qn::TimePeriodRole, QnTimePeriod::fromInterval(now, now));

    menu()->trigger(menu::OpenEventLogAction, parameters);
}

void ActionHandler::at_pingAction_triggered()
{
    const auto host = menu()->currentParameters(sender()).argument(Qn::TextRole).toString();

    if (nx::network::SocketGlobals::addressResolver().isCloudHostname(host))
        return;

    if (nx::build_info::isWindows())
    {
        nx::utils::startProcessDetached("cmd", {"/C", "ping", host, "-t"});
    }
    else if (nx::build_info::isLinux())
    {
        const QStringList terminals{
            "x-terminal-emulator", //< Debian/Ubuntu system-wide alias for default terminal.
            "gnome-terminal", //< Gnome variant.
            "konsole", //< KDE variant.
            "xterm" //< Fallback.
        };
        for (const auto& terminal: terminals)
        {
            const bool result = nx::utils::startProcessDetached(terminal, {"-e", "ping", host});
            if (result)
            {
                NX_DEBUG(this, "Ping %1 started in %2", host, terminal);
                return;
            }

            NX_VERBOSE(this, "Cannot run %1. Error code: %2", terminal, result);
        }
        NX_ERROR(this, "Cannot run `ping` in any of known terminal emulators");
    }
    else if (nx::build_info::isMacOsX())
    {
        auto dialog = new QnPingDialog(nullptr, {Qt::Dialog | Qt::WindowStaysOnTopHint});
        dialog->setHostAddress(host);
        dialog->show();
        dialog->startPings();
    }
}

void ActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = menu()->currentParameters(sender()).resource();
    if (resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(resource->getUrl());
}

void ActionHandler::at_deleteFromDiskAction_triggered()
{
    auto resources = nx::utils::toQSet(menu()->currentParameters(sender()).resources()).values();

    // TODO: #sivanov allow to delete exported layouts.
    // TODO: #ynikitenkov Change texts and dialog when it is enabled.
    QnMessageBox messageBox(
        QnMessageBoxIcon::Warning,
        tr("Confirm files deleting"),
        tr("Are you sure you want to permanently delete these %n files?", "", resources.size()),
        QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes,
        mainWindowWidget());

    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    auto result = messageBox.exec();
    if (result != QDialogButtonBox::Yes)
        return;

    QnFileProcessor::deleteLocalResources(resources);
}

bool ActionHandler::validateResourceName(const QnResourcePtr& resource, const QString& newName) const
{
    /*
     * Users, videowalls and local files should be checked to avoid names collisions.
     * Layouts are checked separately, servers and cameras can have the same name.
     */
    auto checkedFlags = resource->flags() & (Qn::user | Qn::videowall | Qn::local_media);
    if (!checkedFlags)
        return true;

    /* Resource cannot have both of these flags at once. */
    NX_ASSERT(checkedFlags == Qn::user
        || checkedFlags == Qn::videowall
        || checkedFlags == Qn::local_media);

    for (const auto& resource: resourcePool()->getResources())
    {
        if (!resource->hasFlags(checkedFlags))
            continue;

        if (resource->getName().compare(newName, Qt::CaseInsensitive) != 0)
            continue;

        if (checkedFlags == Qn::user)
        {
            QnMessageBox::warning(mainWindowWidget(), tr("There is another user with the same name"));
        }
        else if (checkedFlags == Qn::videowall)
        {
            messages::Videowall::anotherVideoWall(mainWindowWidget());
        }
        else if (checkedFlags == Qn::local_media)
        {
            // Silently skip. Should never get here really, leaving branch for safety.
        }

        return false;
    }

    return true;
}

void ActionHandler::at_renameAction_triggered()
{
    using NodeType = ResourceTree::NodeType;

    const auto parameters = menu()->currentParameters(sender());
    const auto nodeType = parameters.argument<NodeType>(Qn::NodeTypeRole, NodeType::resource);

    QnResourcePtr resource;
    switch (nodeType)
    {
        case NodeType::resource:
        case NodeType::edge:
        case NodeType::recorder:
        case NodeType::sharedLayout:
        case NodeType::sharedResource:
            resource = parameters.resource();
            break;
        default:
            break;
    }
    if (!resource)
        return;

    QnVirtualCameraResourcePtr camera;
    if (nodeType == NodeType::recorder)
    {
        camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            return;
    }

    QString name = parameters.argument<QString>(core::ResourceNameRole).trimmed();
    QString oldName = nodeType == NodeType::recorder
        ? camera->getUserDefinedGroupName()
        : resource->getName();

    if (!NX_ASSERT(!name.isEmpty(), "An attempt to assign empty name to the resource"))
        return;

    if (name == oldName)
        return;

    if (auto layout = resource.dynamicCast<core::LayoutResource>())
    {
        if (layout->isFile())
            renameLocalFile(layout, name);
    }
    else if (nodeType == NodeType::recorder)
    {
        // Recorder name should not be validated.
        QString groupId = camera->getGroupId();

        QnVirtualCameraResourceList modified = resourcePool()->getResources()
            .filtered<QnVirtualCameraResource>(
                [groupId](const auto& camera) { return camera->getGroupId() == groupId; });

        qnResourcesChangesManager->saveCameras(modified,
            [name](const auto& camera) { camera->setUserDefinedGroupName(name); });
    }
    else if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
    {
        server->setName(name);
        qnResourcesChangesManager->saveServer(server, nx::utils::guarded(this,
            [server, oldName](bool success, rest::Handle /*requestId*/)
            {
                if (!success)
                    server->setName(oldName);
            }));
    }
    else if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        qnResourcesChangesManager->saveCamera(camera,
            [name](const auto& camera) { camera->setName(name); });
    }
    else if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        videoWall->setName(name);
        qnResourcesChangesManager->saveVideoWall(videoWall, nx::utils::guarded(this,
            [oldName](bool success, const QnVideoWallResourcePtr& videoWall)
            {
                if (!success)
                    videoWall->setName(oldName);
            }));
    }
    else if (auto webPage = resource.dynamicCast<QnWebPageResource>())
    {
        webPage->setName(name);
        qnResourcesChangesManager->saveWebPage(webPage, nx::utils::guarded(this,
            [oldName](bool success, const QnWebPageResourcePtr& webPage)
            {
                if (!success)
                    webPage->setName(oldName);
            }));
    }
    else if (auto archive = resource.dynamicCast<QnAbstractArchiveResource>())
    {
        if (QFileInfo(name).suffix().isEmpty())
            name.append("." + QFileInfo(oldName).suffix());
        renameLocalFile(archive, name);
    }
    else
    {
        NX_ASSERT(false, "Invalid resource type to rename");
    }
}

void ActionHandler::at_removeFromServerAction_triggered()
{
    QnResourceList resources = menu()->currentParameters(sender()).resources();

    // Layouts and videowalls will be removed in their own handlers.
    // Also separately check each resource.
    resources = resources.filtered(
        [this](const QnResourcePtr& resource)
        {
            return menu()->canTrigger(menu::RemoveFromServerAction, resource)
                && !resource->hasFlags(Qn::layout)
                && !resource->hasFlags(Qn::videowall);
        });

    if (resources.isEmpty())
        return;

    if (ui::messages::Resources::deleteResources(mainWindowWidget(), resources))
    {
        auto callback =
            [this,
                resourcesFailed = QnResourceList(),
                resourcesSuccess = QnResourceList(),
                n = resources.size()](
                    bool result,
                    const QnResourcePtr& resource,
                    nx::network::rest::ErrorId /*errorCode*/) mutable
            {
                if (result)
                    resourcesSuccess << resource;
                else
                    resourcesFailed << resource;

                if ((resourcesFailed.size() + resourcesSuccess.size() == n)
                    && !resourcesFailed.isEmpty())
                {
                    ui::messages::Resources::deleteResourcesFailed(
                        mainWindowWidget(), resourcesFailed);
                }
            };

        for (const auto& resource: resources)
            qnResourcesChangesManager->deleteResource(resource, callback);
    }
}

void ActionHandler::at_saveSessionState_triggered()
{
    appContext()->clientStateHandler()->saveWindowsConfiguration();
    showSessionSavedBanner();
}

void ActionHandler::at_restoreSessionState_triggered()
{
    core::LogonData logonData;
    if (auto connection = this->connection())
        logonData = connection->createLogonData();

    appContext()->clientStateHandler()->restoreWindowsConfiguration(logonData);
}

void ActionHandler::at_deleteSessionState_triggered()
{
    appContext()->clientStateHandler()->deleteWindowsConfiguration();
}

void ActionHandler::showSessionSavedBanner()
{
    SceneBanner::show(tr("Window configuration saved"));
}

void ActionHandler::closeAllWindows()
{
    doCloseApplication(AppClosingMode::CloseAll);
}

void ActionHandler::closeApplication()
{
    doCloseApplication(AppClosingMode::Exit);
}

void ActionHandler::at_clientCommandRequested(
    SharedMemoryData::Command command,
    const QByteArray& data)
{
    switch (command)
    {
        case SharedMemoryData::Command::saveWindowState:
        {
            appContext()->clientStateHandler()->clientRequestedToSaveState();
            showSessionSavedBanner();
            break;
        }
        case SharedMemoryData::Command::restoreWindowState:
        {
            appContext()->clientStateHandler()->clientRequestedToRestoreState(
                QString::fromLatin1(data));
            break;
        }
        case SharedMemoryData::Command::exit:
        {
            if (auto session = system()->session())
                session->autoTerminateIfNeeded();
            doCloseApplication(AppClosingMode::External);
            break;
        }
        default:
            break;
    }
}

void ActionHandler::doCloseApplication(AppClosingMode mode)
{
    if (mode == AppClosingMode::CloseAll)
        appContext()->clientStateHandler()->sharedMemoryManager()->requestToExit();

    if (mode != AppClosingMode::External)
    {
        if (context()->user())
            appContext()->clientStateHandler()->storeSystemSpecificState();

        appContext()->clientStateHandler()->storeSystemIndependentState();
    }

    // User will be forcefully disconnected from the system here.
    menu()->trigger(menu::BeforeExitAction);

    context()->setClosingDown(true);
    mainWindowWidget()->hide();
    qApp->exit(0);
    applauncher::api::scheduleProcessKill(QCoreApplication::applicationPid(),
        PROCESS_TERMINATE_TIMEOUT);
}

void ActionHandler::at_beforeExitAction_triggered() {
    deleteDialogs();
}

QnAdjustVideoDialog* ActionHandler::adjustVideoDialog() {
    return m_adjustVideoDialog.data();
}

void ActionHandler::at_adjustVideoAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;

    QnNonModalDialogConstructor<QnAdjustVideoDialog> dialogConstructor(m_adjustVideoDialog, mainWindowWidget());
    adjustVideoDialog()->setWidget(widget);
}

void ActionHandler::at_createZoomWindowAction_triggered() {
    const auto params = menu()->currentParameters(sender());

    auto widget = params.widget<QnMediaResourceWidget>();
    if (!widget)
        return;

    QRectF rect = params.argument<QRectF>(core::ItemZoomRectRole, QRectF(0.25, 0.25, 0.5, 0.5));
    AddToLayoutParams addParams;
    addParams.position = widget->item()->combinedGeometry().center();
    addParams.zoomWindow = rect;
    addParams.dewarpingParams.enabled = widget->resource()->getDewarpingParams().enabled;  // zoom items on fisheye cameras must always be dewarped
    addParams.zoomUuid = widget->item()->uuid();
    addParams.frameDistinctionColor = params.argument<QColor>(Qn::ItemFrameDistinctionColorRole);
    addParams.zoomWindowRectangleVisible =
        params.argument<bool>(Qn::ItemZoomWindowRectangleVisibleRole);
    addParams.rotation = widget->item()->rotation();
    addParams.displayRoi = widget->item()->displayRoi();
    addParams.displayAnalyticsObjects = widget->item()->displayAnalyticsObjects();
    addParams.displayHotspots = widget->item()->displayHotspots();
    addToLayout(
        workbench()->currentLayoutResource(),
        widget->resource(),
        addParams);
}

void ActionHandler::at_setAsBackgroundAction_triggered() {

    auto checkCondition =
        [this]()
        {
            auto layout = workbench()->currentLayoutResource();
            if (!context()->user() || !NX_ASSERT(layout))
                return false; // action should not be triggered while we are not connected

            if (!ResourceAccessManager::hasPermissions(layout, Qn::EditLayoutSettingsPermission))
                return false;

            if (layout->locked())
                return false;

            return true;
        };

    if (!checkCondition())
        return;

    QPointer<ProgressDialog> progressDialog = createSelfDestructingDialog<ProgressDialog>(mainWindowWidget());
    progressDialog->setWindowTitle(tr("Updating Background..."));
    progressDialog->setText(tr("Image processing may take a few moments. Please be patient."));
    progressDialog->setInfiniteMode();

    const auto parameters = menu()->currentParameters(sender());

    ServerImageCache *cache = new ServerImageCache(system(), this);
    cache->setProperty(uploadingImageARPropertyName, parameters.widget()->aspectRatio());
    connect(cache, &ServerImageCache::fileUploaded, cache, &QObject::deleteLater);
    connect(cache, &ServerImageCache::fileUploaded, this,
        [this, checkCondition, progressDialog]
            (const QString &filename, ServerFileCache::OperationResult status)
        {
            if (!progressDialog || progressDialog->wasCanceled())
                return;

            progressDialog->accept();

            if (!checkCondition())
                return;

            if (status != ServerFileCache::OperationResult::ok)
            {
                QnMessageBox::critical(mainWindowWidget(), tr("Failed to upload image"));
                return;
            }

            setCurrentLayoutBackground(filename);
        });

    cache->storeImage(parameters.resource()->getUrl());
    progressDialog->show();
}

void ActionHandler::setCurrentLayoutBackground(const QString& filename)
{
    QnWorkbenchLayout* wlayout = workbench()->currentLayout();
    QnLayoutResourcePtr layout = wlayout->resource();

    layout->setBackgroundImageFilename(filename);
    if (qFuzzyIsNull(layout->backgroundOpacity()))
        layout->setBackgroundOpacity(0.7);

    wlayout->centralizeItems();
    QRect brect = wlayout->boundingRect();

    // TODO: #sivanov Replace calculations by the corresponding reducer call.

    int minWidth = qMax(brect.width(), LayoutSettingsDialogState::kBackgroundMinSize);
    int minHeight = qMax(brect.height(), LayoutSettingsDialogState::kBackgroundMinSize);

    qreal cellAspectRatio = QnLayoutResource::kDefaultCellAspectRatio;
    if (layout->hasCellAspectRatio())
    {
        const auto spacing = layout->cellSpacing();
        qreal cellWidth = 1.0 + spacing;
        qreal cellHeight = 1.0 / layout->cellAspectRatio() + spacing;
        cellAspectRatio = cellWidth / cellHeight;
    }

    qreal aspectRatio = sender()->property(uploadingImageARPropertyName).toReal();

    int w, h;
    qreal targetAspectRatio = aspectRatio / cellAspectRatio;
    if (targetAspectRatio >= 1.0)
    {
        // width is greater than height
        h = minHeight;
        w = qRound((qreal)h * targetAspectRatio);
        if (w > LayoutSettingsDialogState::kBackgroundMaxSize)
        {
            w = LayoutSettingsDialogState::kBackgroundMaxSize;
            h = qRound((qreal)w / targetAspectRatio);
        }

    }
    else
    {
        w = minWidth;
        h = qRound((qreal)w / targetAspectRatio);
        if (h > LayoutSettingsDialogState::kBackgroundMaxSize)
        {
            h = LayoutSettingsDialogState::kBackgroundMaxSize;
            w = qRound((qreal)h * targetAspectRatio);
        }
    }
    layout->setBackgroundSize(QSize(w, h));
}

void ActionHandler::at_workbench_itemChanged(Qn::ItemRole role)
{
    if (role == Qn::CentralRole && adjustVideoDialog() && adjustVideoDialog()->isVisible())
    {
        QnWorkbenchItem *item = workbench()->item(Qn::CentralRole);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*> (display()->widget(item));
        if (widget)
            adjustVideoDialog()->setWidget(widget);
    }
}

void ActionHandler::at_whatsThisAction_triggered()
{
    if (QWhatsThis::inWhatsThisMode())
        QWhatsThis::leaveWhatsThisMode();
    else
        QWhatsThis::enterWhatsThisMode();
}

void ActionHandler::at_escapeHotkeyAction_triggered()
{
    if (QWhatsThis::inWhatsThisMode())
        QWhatsThis::leaveWhatsThisMode();
}

void ActionHandler::at_browseUrlAction_triggered() {
    QString url = menu()->currentParameters(sender()).argument<QString>(Qn::UrlRole);
    if (url.isEmpty())
        return;

    QDesktopServices::openUrl(nx::Url::fromUserInput(url).toQUrl());
}

void ActionHandler::handleBetaUpdateWarning()
{
    const auto header =
        tr("%1 Beta", "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName());

    const auto demoWarning = setWarningStyleHtml(tr("This build is for demo purposes only."));
    const auto upgradeWarning =
        tr("It cannot be upgraded to subsequent builds as they become available.");

    const auto disconnectSuggestion = tr(
        "To upgrade, please uninstall %1 before installing an updated build (all data will be lost).",
        "%1 is the product name").arg(nx::branding::vmsName());

    const auto content = demoWarning
        + common::html::kLineBreak + upgradeWarning
        + common::html::kLineBreak + disconnectSuggestion;
    QnMessageBox dialog(QnMessageBoxIcon::Warning, header, content,
        QDialogButtonBox::Ok, QDialogButtonBox::Ok, mainWindowWidget());

    dialog.exec();
}

void ActionHandler::at_betaVersionMessageAction_triggered()
{
    if (context()->closingDown())
        return;

    if (ini().developerMode)
        return;

    QString header = tr("Beta version %1").arg(qApp->applicationVersion());
    QString contents = setWarningStyleHtml(tr("This build is for testing purposes only."))
        + common::html::kLineBreak
        + tr("Please upgrade to a next available patch or release version once available.");

    QnMessageBox dialog(QnMessageBoxIcon::Warning, header, contents,
        QDialogButtonBox::Ok, QDialogButtonBox::Ok, mainWindowWidget());

    dialog.exec();
}

void ActionHandler::confirmAnalyticsStorageLocation()
{
    const QnMediaServerResourceList servers = resourcePool()->getResources<QnMediaServerResource>();
    for (const auto& server: servers)
    {
        if (server->metadataStorageId().isNull())
        {
            const auto serverName = server->getName();
            auto storages = server->getStorages();

            nx::utils::erase_if(
                storages,
                [](const auto& s)
                { return !s->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::dbReady); });

            if (storages.empty())
                continue;

            std::sort(storages.begin(), storages.end(),
                [](const QnStorageResourcePtr& a, const QnStorageResourcePtr& b)
                {
                    // TODO: #spanasenko use proper enums
                    bool aMayBeUsed = a->getStorageType() == "local" && a->isWritable();
                    bool bMayBeUsed = b->getStorageType() == "local" && b->isWritable();
                    bool aIsSystem = a->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::system);
                    bool bIsSystem = b->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::system);

                    // Only writable local storages should be used.
                    if (aMayBeUsed && !bMayBeUsed)
                        return true;
                    if (!aMayBeUsed && bMayBeUsed)
                        return false;

                    // System disk is the worst option.
                    if (!aIsSystem && bIsSystem)
                        return true;
                    if (aIsSystem && !bIsSystem)
                        return false;

                    // Prefer storages enabled for writing.
                    if (a->isUsedForWriting() && !b->isUsedForWriting())
                        return true;
                    if (!a->isUsedForWriting() && b->isUsedForWriting())
                        return false;

                    // Than prefer storages that aren't used for backup.
                    if (!a->isBackup() && b->isBackup())
                        return true;
                    if (a->isBackup() && !b->isBackup())
                        return false;

                    // Take the storage with the most space available first.
                    return a->getFreeSpace() > b->getFreeSpace();
                });

            const auto& best = storages.front();

            if (best->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::system))
            {
                const auto defaultDir = storages.empty()
                    ? tr("the largest available partition") //< Should be unreachable, but...
                    : best->getUrl();
                QnMessageBox msgBox(
                    QnMessageBoxIcon::Warning,
                    tr("Confirm storage location for the analytics data on \"%1\"").arg(serverName),
                    tr("The analytics database should only be stored on a local drive"
                        " and can take up large amounts of space."
                        "\n"
                        "Once a location to store analytics data is selected,"
                        " it cannot be easily changed without losing existing data. "
                        "We recommend to choose the location carefully and to avoid using"
                        " the system partition as it may cause severe malfunction."
                        "\n"
                        "By default analytics data will be stored on %1."
                        "\n"
                        "You can select another storage location in the \"Storage Management\" tab"
                        " of the Server Settings dialog."
                    ).arg(defaultDir),
                    QDialogButtonBox::StandardButtons(),
                    QDialogButtonBox::NoButton,
                    mainWindowWidget());

                const auto openSettings = msgBox.addButton(tr("Open Server Settings"),
                    QDialogButtonBox::ButtonRole::ResetRole, Qn::ButtonAccent::NoAccent);
                const auto ok = msgBox.addButton(tr("OK"),
                    QDialogButtonBox::ButtonRole::AcceptRole, Qn::ButtonAccent::Standard);

                msgBox.setEscapeButton(ok); //< Used to close the dialog by ESC / "X" button on Mac

                msgBox.exec();

                if (msgBox.clickedButton() == openSettings)
                {
                    QScopedPointer<QnServerSettingsDialog> dialog(new QnServerSettingsDialog(mainWindowWidget()));
                    dialog->setWindowModality(Qt::ApplicationModal);
                    dialog->setServer(server);
                    dialog->setCurrentPage(QnServerSettingsDialog::StorageManagementPage);
                    dialog->exec();
                }
            }

            if (server->metadataStorageId().isNull() && !storages.empty())
            {
                // User hasn't choose analytics location, so we have to use the best option.
                // Keep this condition in sync with QnStorageListModel.
                if (best->getStatus() == nx::vms::api::ResourceStatus::online
                    && best->getStorageType() == "local")
                {
                    // todo: It is better to do next calls after this call is finished
                    server->setMetadataStorageId(best->getId());
                    server->savePropertiesAsync();
                }
            }
        }
    }
}

void ActionHandler::at_queueAppRestartAction_triggered()
{
    auto tryToRestartClient =
        []
        {
            using namespace nx::vms::applauncher::api;

            /* Try to run applauncher if it is not running. */
            if (!checkOnline())
                return false;

            const auto result = restartClient();
            if (result == ResultType::ok)
                return true;

            static const int kMaxTries = 5;
            for (int i = 0; i < kMaxTries; ++i)
            {
                QThread::msleep(100);
                qApp->processEvents();
                if (restartClient() == ResultType::ok)
                    return true;
            }
            return false;
        };

    if (!tryToRestartClient())
    {
        QnConnectionDiagnosticsHelper::failedRestartClientMessage(mainWindowWidget());
        return;
    }
    menu()->trigger(menu::DelayedForcedExitAction);
}

void ActionHandler::at_selectTimeServerAction_triggered()
{
    openSystemAdministrationDialog(QnSystemAdministrationDialog::TimeServerSelection);
}

void ActionHandler::openInBrowserDirectly(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    // TODO: #akolesnikov #3.1 VMS-2806 deprecate this method, always use openInBrowser()

    if (isCloudServer(server))
        return;

    nx::Url url(server->getApiUrl());
    url.setUserName(QString());
    url.setPassword(QString());
    url.setPath(path);
    url.setFragment(fragment);

    url = QnNetworkProxyFactory::urlToResource(url, server, "proxy");
    QDesktopServices::openUrl(url.toQUrl());
}

void ActionHandler::openInBrowser(const QnMediaServerResourcePtr& server,
    const QString& path, const QString& fragment)
{
    if (!server || !context()->user())
        return;

    // path may contains path + url query params
    // TODO: #akolesnikov #3.1 VMS-2806
    nx::Url serverUrl(server->getApiUrl().toString() + path);
    serverUrl.setFragment(fragment);

    auto systemContext = SystemContext::fromResource(server);
    if (!NX_ASSERT(systemContext) || !NX_ASSERT(systemContext->connection()))
        return;

    const auto credentials = systemContext->connection()->credentials();
    if (const auto token = credentials.authToken; token.isBearerToken())
    {
        return openInBrowser(server, serverUrl, token.value, AuthMethod::bearerToken);
    }

    nx::Url proxyUrl = QnNetworkProxyFactory::urlToResource(serverUrl, server);
    proxyUrl.setPath(lit("/api/getNonce"));

    if (m_serverRequests.find(proxyUrl) == m_serverRequests.end())
    {
        // No other requests to this proxy, so we have to get nonce by ourselves.
        auto reply = new QnAsyncHttpClientReply(
            nx::network::http::AsyncHttpClient::create(
                systemContext->certificateVerifier()->makeAdapterFunc(server->getId(), proxyUrl)),
            this);
        connect(
            reply, &QnAsyncHttpClientReply::finished,
            this, &ActionHandler::at_nonceReceived);

        // TODO: Think of preloader in case of user complains about delay.
        reply->asyncHttpClient()->doGet(proxyUrl);
    }

    m_serverRequests.emplace(proxyUrl, ServerRequest{server, serverUrl});
}

void ActionHandler::at_nonceReceived(QnAsyncHttpClientReply *reply)
{
    std::unique_ptr<QnAsyncHttpClientReply> replyGuard(reply);
    const auto nonceUrl = reply->url();
    auto range = m_serverRequests.equal_range(nonceUrl);
    if (range.first == range.second)
        return;

    std::vector<ServerRequest> requests;
    for (auto it = range.first; it != range.second; ++it)
        requests.push_back(std::move(it->second));

    m_serverRequests.erase(range.first, range.second);

    nx::network::rest::JsonResult result;
    NonceReply auth;
    if (!QJson::deserialize(reply->data(), &result) || !QJson::deserialize(result.reply, &auth))
    {
        QnMessageBox::critical(mainWindowWidget(), tr("Failed to open server web page"));
        return;
    }

    for (const auto& request: requests)
    {
        auto systemContext = SystemContext::fromResource(request.server);
        if (!NX_ASSERT(systemContext) || !NX_ASSERT(systemContext->connection()))
            continue;

        const auto credentials = systemContext->connection()->credentials();

        const auto authParam = createHttpQueryAuthParam(
            QString::fromStdString(credentials.username),
            QString::fromStdString(credentials.authToken.value),
            auth.realm,
            nx::network::http::Method::get,
            auth.nonce.toUtf8());

        openInBrowser(request.server, request.url, authParam.data(), AuthMethod::queryParam);
    }
}

void ActionHandler::openInBrowser(
    const QnMediaServerResourcePtr& server, nx::Url targetUrl,
    std::string auth, AuthMethod authMethod) const
{
    targetUrl = QnNetworkProxyFactory::urlToResource(targetUrl, server);

    const auto gateway = appContext()->cloudGateway();
    nx::network::SocketAddress targetAddress{targetUrl.host(), (uint16_t) targetUrl.port()};
    switch (authMethod)
    {
        case AuthMethod::queryParam:
        {
            QUrlQuery urlQuery(targetUrl.toQUrl());
            urlQuery.addQueryItem("auth", QString::fromStdString(auth));
            targetUrl.setQuery(urlQuery);
            gateway->enforceHeadersFor(targetAddress, {});
            break;
        }
        case AuthMethod::bearerToken:
        {
            if (nx::network::SocketGlobals::addressResolver().isCloudHostname(
                targetAddress.address.toString()))
            {
                targetAddress.port = 0; //< The same as in ProxyHandler::detectProxyTarget.
            }
            nx::network::http::header::BearerAuthorization header(auth);
            gateway->enforceHeadersFor(targetAddress, {{header.NAME, header.toString()}});
            break;
        }
        default:
            NX_ASSERT(false, "Unsupported auth method: %1", static_cast<int>(authMethod));
    }

    // Client local gateway uses SSL regardless of its endpoint, so it never makes sense to open
    // gateway url by https.
    targetUrl = nx::Url(nx::format("https://%1/%2:%3:%4%5?%6").args(
        gateway->endpoint().toString(), targetUrl.scheme(),
        targetUrl.host(), targetUrl.port(),
        targetUrl.path(), targetUrl.query()));

    QDesktopServices::openUrl(targetUrl.toQUrl());
}

void ActionHandler::at_openAdvancedSearchDialog_triggered()
{
    if (!m_advancedSearchDialog)
    {
        m_advancedSearchDialog = new AdvancedSearchDialog(mainWindowWidget());

        const auto parameters = menu()->currentParameters(sender());
        const auto parent = utils::extractParentWidget(parameters, mainWindowWidget());
        m_advancedSearchDialog->window()->setScreen(parent->screen());
    }

    m_advancedSearchDialog->open();
    m_advancedSearchDialog->raise();
}

void ActionHandler::at_openImportFromDevicesDialog_triggered()
{
    auto dialog = new integrations::ImportFromDeviceDialog(mainWindowWidget());
    connect(dialog, &QmlDialogWrapper::done, dialog, &QObject::deleteLater);

    dialog->open();
}

void ActionHandler::deleteDialogs()
{
    if (adjustVideoDialog())
        delete adjustVideoDialog();

    if (cameraListDialog())
        delete cameraListDialog();

    if (systemAdministrationDialog())
        delete systemAdministrationDialog();

    if (m_advancedSearchDialog)
        delete m_advancedSearchDialog.data();

    m_integrationsDialog.reset();
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
