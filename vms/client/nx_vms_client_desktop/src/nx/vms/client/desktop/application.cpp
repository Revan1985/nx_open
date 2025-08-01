// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application.h"

#include <QtCore/QtGlobal>

#ifdef Q_OS_LINUX
    #include <unistd.h>
#endif

#ifdef Q_WS_X11
    #include <X11/Xlib.h>
#endif

#if defined(Q_OS_MACOS)
    #include <sys/sysctl.h>
#endif

#include <iostream>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtGui/QWindow>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtWidgets/QApplication>
#if QT_CONFIG(vulkan)
#include <QtGui/QVulkanInstance>
#include <QtGui/private/qvulkandefaultinstance_p.h>
#endif

#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>
#include <nx/audio/audiodevice.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/kit/output_redirector.h>
#include <nx/media/decoder_registrar.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/speech_synthesizer/text_to_wave_server.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/utils/rlimit.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/window_controller.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <nx/vms/client/desktop/window_context.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/gpu/gpu_devices.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>
#include <ui/utils/user_action_logger.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/combobox_wheel_filter.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#if defined(Q_OS_MACOS)
    #include <ui/workaround/mac_utils.h>
#endif

#if defined(Q_OS_WIN)
    #include <QtGui/qpa/qplatformwindow_p.h>
#endif

namespace nx::vms::client::desktop {

namespace {

const int kSuccessCode = 0;
static constexpr int kStarDragDistance = 20;

void initApplication(const QnStartupParameters& startupParams)
{
    QThread::currentThread()->setPriority(QThread::HighestPriority);

    // Set up application parameters so that QSettings know where to look for settings.
    QApplication::setOrganizationName(nx::branding::company());
    QApplication::setApplicationName(nx::branding::desktopClientInternalName());
    QApplication::setApplicationDisplayName(nx::branding::desktopClientDisplayName());

    QString applicationVersion = !startupParams.engineVersion.isEmpty()
        ? startupParams.engineVersion : nx::build_info::vmsVersion();
    if (!nx::build_info::usedMetaVersion().isEmpty())
        applicationVersion += " " + nx::build_info::usedMetaVersion();

    QApplication::setApplicationVersion(applicationVersion);
    QApplication::setStartDragDistance(kStarDragDistance);

    // We don't want changes in desktop color settings to clash with our custom style.
    QApplication::setDesktopSettingsAware(false);
    QApplication::setQuitOnLastWindowClosed(true);
}

void sendCloudPortalConfirmation(const nx::vms::utils::SystemUri& uri, QObject* owner)
{
    QPointer<QNetworkAccessManager> manager(new QNetworkAccessManager(owner));
        QObject::connect(manager.data(), &QNetworkAccessManager::finished,
            [manager](QNetworkReply* reply)
            {
                reply->deleteLater();
                manager->deleteLater();
            });

        QUrl url(nx::toQString(nx::network::AppInfo::defaultCloudPortalUrl(
            nx::network::SocketGlobals::cloud().cloudHost())));
        url.setPath("/api/utils/visitedKey");

        const QJsonObject data{{QString("key"), uri.authKey()}};
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

        manager->post(request, QJsonDocument(data).toJson(QJsonDocument::Compact));
}

Qt::WindowFlags calculateWindowFlags()
{
    if (qnRuntime->isAcsMode())
        return Qt::Window;

    Qt::WindowFlags result = nx::build_info::isMacOsX()
        ? Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
        : Qt::Window | Qt::CustomizeWindowHint;

    if (qnRuntime->isVideoWallMode() && !ini().showBorderInVideoWallMode)
        result |= Qt::FramelessWindowHint | Qt::BypassWindowManagerHint;

    return result;
}

void initQmlGlyphCacheWorkaround()
{
    #if defined (Q_OS_MACOS)
        #if defined(__amd64)
            // Fixes issue with QML texts rendering observed on the MacBook laptops with
            // Nvidia GeForce GT 750M discrete graphic hardware and all M1 macs.
            size_t len = 0;
            sysctlbyname("hw.model", nullptr, &len, nullptr, 0);
            QByteArray hardwareModel(len, 0);
            sysctlbyname("hw.model", hardwareModel.data(), &len, nullptr, 0);

            // Detect running under Rosetta2.
            // https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment
            const auto processIsTranslated =
                []() -> bool
                {
                    int ret = 0;
                    size_t size = sizeof(ret);
                    if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
                        return errno != ENOENT; //< Assume native only when ENOENT is returned.
                    return (bool) ret;
                };

            const bool glyphcacheWorkaround =
                hardwareModel.contains("MacBookPro11,2")
                || hardwareModel.contains("MacBookPro11,3")
                || processIsTranslated();
        #else
            // Currently all Apple Silicon macs require this.
            const bool glyphcacheWorkaround = true;
        #endif
    #else
        const bool glyphcacheWorkaround = false;
    #endif

    if (glyphcacheWorkaround)
        qputenv("QML_USE_GLYPHCACHE_WORKAROUND", "1");
}

void askForGraphicsApiSubstitution()
{
    #if defined (Q_OS_WINDOWS)
        const QDialogButtonBox::StandardButton selectedButton = QnMessageBox::question(
            nullptr,
            QCoreApplication::translate(
                "runApplication",
                "Would you like to try switching to DirectX?"),
            "",
            QDialogButtonBox::Yes | QDialogButtonBox::No);

        if (selectedButton == QDialogButtonBox::Yes)
            appContext()->runtimeSettings()->setGraphicsApi(GraphicsApi::direct3d11);
    #endif
}

// This function is called BEFORE QApplication is created.
void setGraphicsSettingsEarly(const QnStartupParameters& startupParams)
{
    if (build_info::isLinux())
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "xcb");
    }

    initQmlGlyphCacheWorkaround();

    // NativeTextRendering is required for the equal text rendering on widgets and qml.
    if (nx::build_info::isMacOsX() || nx::build_info::isLinux())
        QQuickWindow::setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);

    // This attribute is needed to embed QQuickWidget into other QWidgets.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    if (ini().roundDpiScaling)
    {
        QApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::Round);
    }

    if (nx::build_info::isMacOsX())
    {
        // This should go into QnClientModule::initSurfaceFormat(),
        // but we must set OpenGL version before creation of GUI application.

        QSurfaceFormat format;
        // Mac computers OpenGL versions:
        //   https://support.apple.com/en-us/HT202823
        format.setProfile(QSurfaceFormat::CoreProfile);
        // Chromium requires OpenGL 4.1 on macOS for WebGL and other HW accelerated staff.
        format.setVersion(4, 1);

        QSurfaceFormat::setDefaultFormat(format);
    }

    GraphicsApi graphicsApi = GraphicsApi::autoselect;

    if (!NX_ASSERT(nx::reflect::enumeration::fromString(ini().graphicsApi, &graphicsApi)))
        return;

    if (nx::build_info::isLinux())
    {
        static const char* kXcbGlIntegration = "QT_XCB_GL_INTEGRATION";

        // EGL on arm64 causes issues with OpenGL on Nvidia - QML windows are rendered empty.
        if (graphicsApi != GraphicsApi::legacyopengl && !nx::build_info::isArm())
        {
            // HW decoding through VAAPI requires EGL.
            if (qgetenv(kXcbGlIntegration).isEmpty())
                qputenv(kXcbGlIntegration, "xcb_egl");
        }

        if (qgetenv(kXcbGlIntegration) == "xcb_egl")
        {
            // Try to select GPU via EGL here - we cannot do this after QApplication is created.
            // We can decide to switch to Vulkan later.
            gpu::selectDevice(GraphicsApi::opengl, ini().gpuName);
        }

        // Pass special env variables to enable vulkan video decoding.
        // This should be done before creation of vulkan instance.
        static const char* kEnableVulkanVideoIntel = "ANV_VIDEO_DECODE";
        static const char* kEnableVulkanVideoAmd = "RADV_PERFTEST";

        if (qgetenv(kEnableVulkanVideoIntel).isEmpty())
            qputenv(kEnableVulkanVideoIntel, "1");
        if (qgetenv(kEnableVulkanVideoAmd).isEmpty())
            qputenv(kEnableVulkanVideoAmd, "video_decode");
    }
}

// This function is called AFTER QApplication is created.
void setGraphicsSettings()
{
    static const QHash<GraphicsApi, QSGRendererInterface::GraphicsApi> nameToApi =
    {
        {GraphicsApi::legacyopengl, QSGRendererInterface::OpenGL},
        {GraphicsApi::software, QSGRendererInterface::Software},
        {GraphicsApi::opengl, QSGRendererInterface::OpenGL},
        {GraphicsApi::direct3d11, QSGRendererInterface::Direct3D11},
        {GraphicsApi::direct3d12, QSGRendererInterface::Direct3D12},
        {GraphicsApi::vulkan, QSGRendererInterface::Vulkan},
        {GraphicsApi::metal, QSGRendererInterface::Metal},
    };

    auto graphicsApi = appContext()->runtimeSettings()->graphicsApi();

    if (graphicsApi == GraphicsApi::autoselect)
    {
        if (nx::build_info::isMacOsX())
        {
            graphicsApi = GraphicsApi::metal;
        }
        else if (nx::build_info::isWindows())
        {
            graphicsApi = GraphicsApi::direct3d11;
        }
        else
        {
            graphicsApi = gpu::isVulkanVideoSupported()
                ? GraphicsApi::vulkan
                : GraphicsApi::opengl;
        }
    }

    const auto gpuInfo = gpu::selectDevice(graphicsApi, ini().gpuName);

    if (gpuInfo.name.toLower().contains("nvidia") && graphicsApi == GraphicsApi::vulkan)
    {
        // Nvidia drivers cannot create more than 5 VK devices, so enable device sharing.
        static const char* kVkDeviceShared = "QT_VK_DEVICE_SHARED";

        if (qgetenv(kVkDeviceShared).isEmpty())
            qputenv(kVkDeviceShared, "1");

        // Chromium blocks WebGL on mobile tegra when using ANGLE vulkan backend.
        if (nx::build_info::isArm())
        {
            static const char* kChromiumFlags = "QTWEBENGINE_CHROMIUM_FLAGS";
            auto value = qgetenv(kChromiumFlags);
            qputenv(kChromiumFlags, value + " --ignore-gpu-blocklist");
        }
    }

    if (nx::build_info::isLinux()
        && gpuInfo.name.toLower().contains("intel")
        && appContext()->runtimeSettings()->graphicsApi() == GraphicsApi::autoselect)
    {
        // Never autoselect Intel+Vulkan on Linux - vulkan video support is not reliable.
        graphicsApi = GraphicsApi::opengl;
    }

    if (graphicsApi == GraphicsApi::vulkan && nx::build_info::isLinux())
    {
        // Workaround for QTBUG-123607.
        static const char* kDisableNativePixmaps = "QTWEBENGINE_DISABLE_NATIVE_PIXMAPS";

        if (qgetenv(kDisableNativePixmaps).isEmpty())
            qputenv(kDisableNativePixmaps, "1");
    }

    if (graphicsApi == GraphicsApi::metal
        || graphicsApi == GraphicsApi::direct3d11
        || graphicsApi == GraphicsApi::direct3d12)
    {
        // In Qt 6.9.1 there is an issue with our client application: the rendering thread
        // synchronization in case of several visible QML windows and enabled VSync with these
        // backends is causing prolonged locks of the GUI thread.
        qputenv("QSG_RENDER_LOOP", "basic");
    }

    const auto selectedApi = nameToApi.value(graphicsApi, QSGRendererInterface::OpenGL);
    QQuickWindow::setGraphicsApi(selectedApi);
    appContext()->runtimeSettings()->setGraphicsApi(graphicsApi);

    QQuickWindow::setDefaultAlphaBuffer(true);
}

} // namespace

int runApplicationInternal(QApplication* application, const QnStartupParameters& startupParams)
{
    ApplicationContext::Mode applicationMode = startupParams.selfUpdateMode
        ? ApplicationContext::Mode::selfUpdate
        : ApplicationContext::Mode::desktopClient;

    ApplicationContext::Features features = startupParams.selfUpdateMode
        ? ApplicationContext::Features::none()
        : ApplicationContext::Features::all();

    auto applicationContext = std::make_unique<ApplicationContext>(
        applicationMode,
        features,
        startupParams);

    /* Running updater after QApplication and logging are initialized. */
    if (qnRuntime->isDesktopMode() && !startupParams.exportedMode)
    {
        /* All functionality is in the constructor. */
        SelfUpdater updater(startupParams);
    }

    /* Immediately exit if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return kSuccessCode;

    // TODO: #sivanov Move QnClientModule contents to Application Context and System Context.
    QnClientModule client(applicationContext->currentSystemContext(), startupParams);

    NX_INFO(NX_SCOPE_TAG, "IniConfig iniFilesDir: %1",  nx::kit::IniConfig::iniFilesDir());

    const auto graphicsApi = appContext()->runtimeSettings()->graphicsApi();

    if (graphicsApi == GraphicsApi::opengl || graphicsApi == GraphicsApi::legacyopengl)
    {
        if (!QnGLCheckerInstrument::checkGLHardware())
            askForGraphicsApiSubstitution();
    }

    setGraphicsSettings();

    client.initWebEngine();

    if (startupParams.customUri.isValid())
        sendCloudPortalConfirmation(startupParams.customUri, application);

    /* Initialize sound. */
    nx::audio::AudioDevice::instance()->setVolume(appContext()->localSettings()->audioVolume());

    qApp->installEventFilter(&HelpHandler::instance());

    // Hovered QComboBox changes its value when user scrolls a mouse wheel, even if the ComboBox
    // is not focused. It leads to weird and undesirable UI behaviour in some parts of the client.
    // We use a global Event Filter to prevent QComboBox instances from receiving Wheel events.
    ComboboxWheelFilter wheelFilter;
    qApp->installEventFilter(&wheelFilter);

    auto windowContext = std::make_unique<WindowContext>();
    applicationContext->addWindowContext(windowContext.get());

    qApp->installEventFilter(UserActionsLogger::instance());

    #if defined(Q_OS_LINUX)
        qputenv("RESOURCE_NAME", nx::branding::brand().toUtf8());
    #endif

    // Dealing with EULA in videowall mode can make people frown.
    if (!qnRuntime->isVideoWallMode())
    {
        int accepted = appContext()->localSettings()->acceptedEulaVersion();
        int current = nx::branding::eulaVersion();
        const bool showEula = accepted < current && qgetenv("VMS_ACCEPT_EULA") != "YES";

        if (showEula && !EulaDialog::acceptEulaFromFile(":/license.html", current))
        {
            // We should exit completely.
            return 0;
        }
    }

    /* Create main window. */

    QScopedPointer<MainWindow> mainWindow(
        new MainWindow(windowContext.get(), /*parent*/ nullptr, calculateWindowFlags()));
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    #if defined(Q_OS_LINUX)
        qunsetenv("RESOURCE_NAME");
    #endif

    nx::media::DecoderRegistrar::registerDecoders({});

    // Window handle must exist before events processing. This is required to initialize the scene.
    [[maybe_unused]] volatile auto winId = mainWindow->winId();

    if (qnRuntime->isDesktopMode())
    {
        auto geometryManager = std::make_unique<WindowGeometryManager>(
            std::make_unique<WindowController>(mainWindow.get()));
        applicationContext->clientStateHandler()->registerDelegate(
            WindowGeometryManager::kWindowGeometryData,
            std::move(geometryManager));
    }

    // We must handle all 'WindowScreenChange' events _before_ we move window.
    qApp->processEvents();

    applicationContext->clientStateHandler()->clientStarted(
        StartupParameters::fromCommandLineParams(startupParams));

    #if defined(Q_OS_WIN)
        if (qnRuntime->isVideoWallMode() && !ini().showBorderInVideoWallMode)
        {
            mainWindow->windowHandle()
                ->nativeInterface<QNativeInterface::Private::QWindowsWindow>()
                ->setHasBorderInFullScreen(true);
        }
    #endif

    mainWindow->show();
    mainWindow->updateDecorationsState();

    applicationContext->initializeDesktopCamera(
        qobject_cast<QOpenGLWidget*>(mainWindow->viewport()));
    client.startLocalSearchers();

    windowContext->handleStartupParameters(startupParams);

    int result = application->exec();

    applicationContext->clientStateHandler()->unregisterDelegate(
        WindowGeometryManager::kWindowGeometryData);

    /* Write out settings. */
    appContext()->localSettings()->audioVolume = nx::audio::AudioDevice::instance()->volume();
    nx::audio::AudioDevice::instance()->deinitialize();

    // A workaround to ensure no cross-system connections are made after subsequent stopAll call.
    if (auto csw = applicationContext->cloudStatusWatcher())
        csw->suppressCloudInteraction({});
    if (auto ccsm = applicationContext->cloudCrossSystemManager())
        ccsm->resetCloudSystems(/*enableCloudSystems*/ false);

    // Stop all long runnables before destroying window context.
    applicationContext->stopAll();

    // Wait while deleteLater objects will be freed
    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();

    return result;
}

int runApplication(int argc, char** argv)
{
    nx::kit::OutputRedirector::ensureOutputRedirection();

#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

    // TODO: Qt6 workaround for broken macOS style of QuickControls2.
    // https://forum.qt.io/topic/131823/lots-of-typeerrors-in-console-when-migrating-to-qt6/5
    // https://bugreports.qt.io/browse/QTBUG-98098
    QQuickStyle::setStyle("Basic");

    nx::utils::rlimit::setMaxFileDescriptors(8000);

    const QnStartupParameters startupParams = QnStartupParameters::fromCommandLineArg(argc, argv);

    setGraphicsSettingsEarly(startupParams);

    auto application = std::make_unique<QApplication>(argc, argv);
    initApplication(startupParams);

    // Initialize speech synthesis.
    const QString applicationDirPath = QCoreApplication::applicationDirPath();
    NX_ASSERT(!applicationDirPath.isEmpty(), "QApplication may not have been initialized.");
    auto textToWaveServer = std::make_unique<nx::speech_synthesizer::TextToWaveServer>(
        applicationDirPath);
    textToWaveServer->start();
    textToWaveServer->waitForStarted();

    // Adding exe dir to plugin search path.
    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths(pluginDirs);
#ifdef Q_OS_LINUX
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, lit("/etc/xdg"));
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, lit("/etc/xdg"));
#endif

#ifdef Q_OS_MAC
    mac_restoreFileAccess();
#endif

    int result = runApplicationInternal(application.get(), startupParams);

#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

    return result;
}

} // namespace nx::vms::client::desktop {
