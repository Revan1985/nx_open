// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_context.h"

#include <QtWidgets/QApplication>

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_startup_parameters.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/joystick/settings/manager.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/system_logon/logic/context_current_user_watcher.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/client/desktop/system_update/server_update_tool.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/system_settings.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/statistics/modules/actions_statistics_module.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/statistics/modules/durations_statistics_module.h>
#include <ui/statistics/modules/generic_statistics_module.h>
#include <ui/statistics/modules/graphics_statistics_module.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>
#include <ui/statistics/modules/users_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/watchers/workbench_desktop_camera_watcher.h>
#include <ui/workbench/watchers/workbench_layout_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench.h>

#if defined(Q_OS_LINUX)
    #include <ui/workaround/x11_launcher_workaround.h>
#endif

using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::client::desktop;

QnWorkbenchContext::QnWorkbenchContext(QnWorkbenchAccessController* accessController, QObject* parent):
    QObject(parent),
    m_accessController(accessController),
    m_userWatcher(nullptr),
    m_layoutWatcher(nullptr),
    m_closingDown(false)
{
    /* Layout watcher should be instantiated before snapshot manager because it can modify layout on adding. */
    m_layoutWatcher = instance<QnWorkbenchLayoutWatcher>();
    m_snapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(this));

    m_workbench.reset(new QnWorkbench(this));

    m_userWatcher = instance<ContextCurrentUserWatcher>();

    // We need to instantiate core user watcher for two way audio availability watcher.
    const auto coreUserWatcher = commonModule()->instance<nx::vms::client::core::UserWatcher>();

    // Desktop camera must work in the normal mode only.
    if (qnRuntime->isDesktopMode())
        instance<QnWorkbenchDesktopCameraWatcher>();

    connect(m_userWatcher, &ContextCurrentUserWatcher::userChanged, this,
        [this, coreUserWatcher](const QnUserResourcePtr& user)
        {
            coreUserWatcher->setUser(user);
            if (m_accessController)
                m_accessController->setUser(user);
            emit userChanged(user);
            emit userIdChanged();
        });

    /* Create dependent objects. */
    m_synchronizer.reset(new QnWorkbenchSynchronizer(this));

    m_menu.reset(new ui::action::Manager(this));
    m_joystickManager.reset(joystick::Manager::create(this));
    m_display.reset(new QnWorkbenchDisplay(this));
    m_navigator.reset(new QnWorkbenchNavigator(this));

    // Adds statistics modules

    auto statisticsManager = commonModule()->instance<QnStatisticsManager>();

    const auto genericStatModule = instance<QnGenericStatisticsModule>();
    statisticsManager->registerStatisticsModule("generic", genericStatModule);

    const auto actionsStatModule = instance<QnActionsStatisticsModule>();
    actionsStatModule->setActionManager(m_menu.data());
    statisticsManager->registerStatisticsModule(lit("actions"), actionsStatModule);

    const auto userStatModule = instance<QnUsersStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("users"), userStatModule);

    const auto graphicsStatModule = instance<QnGraphicsStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("graphics"), graphicsStatModule);

    const auto durationStatModule = instance<QnDurationStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("durations"), durationStatModule);

    const auto sessionRestoreStatModule = instance<QnSessionRestoreStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("restore"), sessionRestoreStatModule);

    statisticsManager->registerStatisticsModule("certificate",
        ApplicationContext::instance()->certificateStatisticsModule());

    statisticsManager->registerStatisticsModule("controls",
        ApplicationContext::instance()->controlsStatisticsModule());

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
        statisticsManager, &QnStatisticsManager::resetStatistics);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived,
        statisticsManager, &QnStatisticsManager::sendStatistics);

    instance<nx::vms::client::desktop::SystemHealthState>();
    instance<QnGLCheckerInstrument>();

    instance<workbench::LocalNotificationsManager>();

    instance<nx::vms::client::desktop::Director>();
    instance<nx::vms::client::desktop::ServerUpdateTool>();
    instance<nx::vms::client::desktop::WorkbenchUpdateWatcher>();
    instance<nx::vms::client::desktop::ClientUpdateManager>();
    m_resourceTreeSettings.reset(new nx::vms::client::desktop::ResourceTreeSettings());

    initWorkarounds();
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    /* Destroy typed subobjects in reverse order to how they were constructed. */
    QnInstanceStorage::clear();

    m_userWatcher = nullptr;
    m_layoutWatcher = nullptr;
    m_accessController = nullptr;

    /* Destruction order of these objects is important. */
    m_resourceTreeSettings.reset();
    m_navigator.reset();
    m_display.reset();
    m_menu.reset();
    m_snapshotManager.reset();
    m_synchronizer.reset();
    m_workbench.reset();
}

QnWorkbench* QnWorkbenchContext::workbench() const
{
    return m_workbench.data();
}

QnWorkbenchLayoutSnapshotManager* QnWorkbenchContext::snapshotManager() const
{
    return m_snapshotManager.data();
}

nx::vms::client::desktop::ui::action::Manager* QnWorkbenchContext::menu() const
{
    return m_menu.data();
}

QnWorkbenchAccessController* QnWorkbenchContext::accessController() const
{
    return m_accessController;
}

void QnWorkbenchContext::setAccessController(QnWorkbenchAccessController* value)
{
    m_accessController = value;
}

QnWorkbenchDisplay* QnWorkbenchContext::display() const
{
    return m_display.data();
}

QnWorkbenchNavigator* QnWorkbenchContext::navigator() const
{
    return m_navigator.data();
}

nx::vms::client::desktop::joystick::Manager* QnWorkbenchContext::joystickManager() const
{
    return m_joystickManager.get();
}

MainWindow* QnWorkbenchContext::mainWindow() const
{
    return m_mainWindow.data();
}

QWidget* QnWorkbenchContext::mainWindowWidget() const
{
    return mainWindow();
}

void QnWorkbenchContext::setMainWindow(MainWindow* mainWindow)
{
    if (m_mainWindow == mainWindow)
        return;

    m_mainWindow = mainWindow;
    emit mainWindowChanged();
}

ResourceTreeSettings* QnWorkbenchContext::resourceTreeSettings() const
{
    return m_resourceTreeSettings.get();
}

nx::core::Watermark QnWorkbenchContext::watermark() const
{
    if (globalSettings()->watermarkSettings().useWatermark
        && !accessController()->hasGlobalPermission(nx::vms::api::GlobalPermission::admin)
        && user()
        && !user()->getName().isEmpty())
    {
        return {globalSettings()->watermarkSettings(), user()->getName()};
    }
    return {};
}

QAction *QnWorkbenchContext::action(const action::IDType id) const {
    return m_menu->action(id);
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    if(m_userWatcher) {
        return m_userWatcher->user();
    } else {
        return QnUserResourcePtr();
    }
}

QString QnWorkbenchContext::userId() const
{
    const auto userPtr = user();
    return userPtr ? userPtr->getId().toString() : "";
}

void QnWorkbenchContext::setUserName(const QString &userName) {
    if(m_userWatcher)
        m_userWatcher->setUserName(userName);
}

bool QnWorkbenchContext::closingDown() const
{
    return m_closingDown;
}

void QnWorkbenchContext::setClosingDown(bool value)
{
    m_closingDown = value;
}

void QnWorkbenchContext::handleStartupParameters(const QnStartupParameters& startupParams)
{
    menu()->trigger(action::ProcessStartupParametersAction,
        {Qn::StartupParametersRole, startupParams});
}

void QnWorkbenchContext::initWorkarounds()
{
    action::IDType effectiveMaximizeActionId = action::FullscreenAction;
#ifdef Q_OS_LINUX
    /* In Ubuntu its launcher is configured to be shown when a non-fullscreen window has appeared.
    * In our case it means that launcher overlaps our fullscreen window when the user opens any dialogs.
    * To prevent such overlapping there was an attempt to hide unity launcher when the main window
    * has been activated. But now we can't hide launcher window because there is no any visible window for it.
    * Unity-3D launcher is like a 3D-effect activated by compiz window manager.
    * We can investigate possibilities of changing the behavior of unity compiz plugin but now
    * we just disable fullscreen for unity-3d desktop session.
    */
    if (QnX11LauncherWorkaround::isUnity3DSession())
        effectiveMaximizeActionId = action::MaximizeAction;
#endif
    menu()->registerAlias(action::EffectiveMaximizeAction, effectiveMaximizeActionId);
}

bool QnWorkbenchContext::isWorkbenchVisible() const
{
    return m_mainWindow && m_mainWindow->isWorkbenchVisible();
}
