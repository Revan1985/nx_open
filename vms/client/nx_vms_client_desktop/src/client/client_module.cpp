// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_module.h"

#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QResource>
#include <QtCore/QStandardPaths>
#include <QtGui/QSurfaceFormat>
#include <QtQml/QQmlEngine>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>

#include <api/network_proxy_factory.h>
#include <client/client_runtime_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/network/socket_global.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/analytics_metadata_provider_factory.h>
#include <nx/vms/client/core/analytics/analytics_settings_manager.h>
#include <nx/vms/client/core/analytics/analytics_settings_manager_factory.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/license_health_watcher.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/command_line_parser.h>

#if defined(Q_OS_WIN)
    #include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/windows_desktop_resource.h>
#endif

#if defined(Q_OS_MAC)
    #include <ui/workaround/mac_utils.h>
#endif

namespace nx::vms::client::desktop {

static QnClientModule* s_instance = nullptr;

struct QnClientModule::Private
{
    void initLicensesModule(SystemContext* systemContext)
    {
        using namespace nx::vms::license;

        videoWallLicenseUsageHelper = std::make_unique<VideoWallLicenseUsageHelper>(systemContext);
        videoWallLicenseUsageHelper->setCustomValidator(
            std::make_unique<license::VideoWallLicenseValidator>(systemContext));
    }

    const QnStartupParameters startupParameters;
    std::unique_ptr<core::AnalyticsSettingsManager> analyticsSettingsManager;
    std::unique_ptr<core::analytics::AnalyticsMetadataProviderFactory> analyticsMetadataProviderFactory;
    std::unique_ptr<LicenseHealthWatcher> licenseHealthWatcher;
    std::unique_ptr<core::analytics::AttributeHelper> analyticsAttributeHelper;
    std::unique_ptr<nx::vms::license::VideoWallLicenseUsageHelper> videoWallLicenseUsageHelper;
    std::unique_ptr<DebugInfoStorage> debugInfoStorage;
};

QnClientModule::QnClientModule(
    SystemContext* systemContext,
    const QnStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.startupParameters = startupParameters})
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    d->analyticsSettingsManager = core::AnalyticsSettingsManagerFactory::createAnalyticsSettingsManager(
        systemContext);

    d->analyticsMetadataProviderFactory.reset(new core::analytics::AnalyticsMetadataProviderFactory());
    d->analyticsMetadataProviderFactory->registerMetadataProviders();

    integrations::initialize(this);

    d->licenseHealthWatcher.reset(new LicenseHealthWatcher(
        systemContext->licensePool()));

    d->debugInfoStorage = std::make_unique<DebugInfoStorage>();

    d->initLicensesModule(systemContext);
    appContext()->moduleDiscoveryManager()->start(systemContext->resourcePool());

    initSurfaceFormat();
}

QnClientModule::~QnClientModule()
{
    // Restoring default message handler.
    nx::disableQtMessageAsserts();

    if (s_instance == this)
        s_instance = nullptr;
}

QnClientModule* QnClientModule::instance()
{
    return s_instance;
}

void QnClientModule::startLocalSearchers()
{
    appContext()->resourceDiscoveryManager()->start();
}

void QnClientModule::initSurfaceFormat()
{
    // Warning: do not set version or profile here.

    auto format = QSurfaceFormat::defaultFormat();

    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoMultisampling))
        format.setSamples(2);
    format.setSwapBehavior(appContext()->localSettings()->glDoubleBuffer()
        ? QSurfaceFormat::DoubleBuffer
        : QSurfaceFormat::SingleBuffer);
    format.setSwapInterval(ini().limitFrameRate ? 1 : 0);

    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    QSurfaceFormat::setDefaultFormat(format);
}

void QnClientModule::initWebEngine()
{
    // QtWebEngine uses a dedicated process to handle web pages. That process needs to know from
    // where to load libraries. It's not a problem for release packages since everything is
    // gathered in one place, but it is a problem for development builds. The simplest solution for
    // this is to set library search path variable. In Linux this variable is needed only for
    // QtWebEngine::defaultSettings() call which seems to create a WebEngine process. After the
    // variable could be restored to the original value. In macOS it's needed for every web page
    // constructor, so we just set it for the whole lifetime of Client application.

    const QByteArray libraryPathVariable =
        nx::build_info::isLinux()
            ? "LD_LIBRARY_PATH"
            : nx::build_info::isMacOsX()
                ? "DYLD_LIBRARY_PATH"
                : "";

    const QByteArray originalLibraryPath =
        libraryPathVariable.isEmpty() ? QByteArray() : qgetenv(libraryPathVariable);

    if (!libraryPathVariable.isEmpty())
    {
        QString libPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../lib");
        if (!originalLibraryPath.isEmpty())
        {
            libPath += ':';
            libPath += originalLibraryPath;
        }

        qputenv(libraryPathVariable, libPath.toLocal8Bit());
    }

    qputenv("QTWEBENGINE_DIALOG_SET", "QtQuickControls2");

    const auto settings = QWebEngineProfile::defaultProfile()->settings();
    // We must support Flash for some camera admin pages to work.
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    // TODO: Add ini parameters for WebEngine attributes
    //settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    //settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    if (!nx::build_info::isMacOsX())
    {
        if (!originalLibraryPath.isEmpty())
            qputenv(libraryPathVariable, originalLibraryPath);
        else
            qunsetenv(libraryPathVariable);
    }
}

QnStartupParameters QnClientModule::startupParameters() const
{
    return d->startupParameters;
}

nx::vms::license::VideoWallLicenseUsageHelper* QnClientModule::videoWallLicenseUsageHelper() const
{
    return d->videoWallLicenseUsageHelper.get();
}

core::AnalyticsSettingsManager* QnClientModule::analyticsSettingsManager() const
{
    return d->analyticsSettingsManager.get();
}

} // namespace nx::vms::client::desktop
