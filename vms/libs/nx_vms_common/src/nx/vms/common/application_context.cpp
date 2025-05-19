// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QCoreApplication>

#include <core/resource/storage_plugin_factory.h>
#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <nx/branding.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/metric/application_metrics_storage.h>
#include <nx/network/cloud/tunnel/tcp/tunnel_tcp_endpoint_verificator_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/common/common_meta_types.h>
#include <utils/common/instance_storage.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>

// Resources initialization must be located outside of the namespace.
static void initializeResources()
{
    Q_INIT_RESOURCE(nx_vms_common);
}

namespace nx::vms::common {

using namespace nx::network;
using namespace nx::metric;

static ApplicationContext* s_instance = nullptr;

struct ApplicationContext::Private
{
    ApplicationContext* const q;
    const PeerType localPeerType;
    const Features features;

    mutable nx::Mutex mutex;
    QMap<nx::Uuid, int> longToShortInstanceId;
    QString locale{nx::branding::defaultLocale()};

    std::unique_ptr<QnLongRunnablePool> longRunnablePool;
    std::unique_ptr<QnLongRunableCleanup> longRunableCleanup;
    std::unique_ptr<QnSyncTime> syncTime;
    std::unique_ptr<QnStoragePluginFactory> storagePluginFactory;
    std::unique_ptr<nx::utils::TimerManager> timerManager;
    std::unique_ptr<ApplicationMetricsStorage> metricsStorage;
    std::unique_ptr<nx::i18n::TranslationManager> translationManager;
    cloud::tcp::EndpointVerificatorFactory::Function endpointVerificatorFactoryBak;

    void initNetworking(const QString& customCloudHost)
    {
        SocketGlobals::init(/*initializationFlags*/ 0, customCloudHost.toStdString());
        SocketGlobals::addressResolver().addFixedAddress("localhost",
            SocketAddress::anyPrivateAddress);

        // Providing mediaserver-specific way of validating peer id.
        endpointVerificatorFactoryBak =
            cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
                [](const std::string& connectSessionId)
                    -> std::unique_ptr<cloud::tcp::AbstractEndpointVerificator>
                {
                    return std::make_unique<CloudMediaServerEndpointVerificator>(
                        connectSessionId);
                });
    }

    void deinitNetworking()
    {
        cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            std::move(endpointVerificatorFactoryBak));
        SocketGlobals::addressResolver().removeFixedAddress("localhost",
            SocketAddress::anyPrivateAddress);
        SocketGlobals::deinit();
    }
};

ApplicationContext::ApplicationContext(
    PeerType localPeerType,
    Features features,
    const QString& customCloudHost,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .q = this,
        .localPeerType = localPeerType,
        .features = features})
{
    if (NX_ASSERT(!s_instance))
        s_instance = this;

    // This instantiation is required to put non-template methods of the QnInstanceStorage class
    // to the export table.
    QnInstanceStorage instanceStorage;

    initializeResources();
    initializeMetaTypes();

    d->longRunnablePool = std::make_unique<QnLongRunnablePool>();
    d->longRunableCleanup = std::make_unique<QnLongRunableCleanup>();
    QnFfmpegHelper::registerLogCallback();

    if (features.flags.testFlag(FeatureFlag::networking))
        d->initNetworking(customCloudHost);

    d->syncTime = std::make_unique<QnSyncTime>();
    d->storagePluginFactory = std::make_unique<QnStoragePluginFactory>();
    d->timerManager = std::make_unique<nx::utils::TimerManager>("CommonTimerManager");
    d->metricsStorage = std::make_unique<ApplicationMetricsStorage>();

    if (features.flags.testFlag(FeatureFlag::translations))
        d->translationManager = std::make_unique<nx::i18n::TranslationManager>();
}

void ApplicationContext::stopAll()
{
    NX_VERBOSE(this, "Stop");

    d->longRunnablePool->stopAll();
}

ApplicationContext::~ApplicationContext()
{
    d->longRunnablePool->stopAll();

    // Long runnables with async destruction should be destroyed before we deinitialize networking.
    d->longRunableCleanup.reset();
    d->longRunnablePool.reset();

    if (d->features.flags.testFlag(FeatureFlag::networking))
        d->deinitNetworking();

    d->translationManager.reset();

    if (NX_ASSERT(s_instance == this))
        s_instance = nullptr;
}

const ApplicationContext::Features& ApplicationContext::commonFeatures() const
{
    return d->features;
}

ApplicationContext* ApplicationContext::instance()
{
    return s_instance;
}

nx::vms::api::PeerType ApplicationContext::localPeerType() const
{
    return d->localPeerType;
}

QString ApplicationContext::locale() const
{
    return d->locale;
}

void ApplicationContext::setLocale(const QString& value)
{
    d->locale = value;
}

nx::i18n::TranslationManager* ApplicationContext::translationManager() const
{
    return d->translationManager.get();
}

void ApplicationContext::setModuleShortId(const nx::Uuid& id, int number)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->longToShortInstanceId.insert(id, number);
}

int ApplicationContext::moduleShortId(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ? itr.value() : -1;
}

QString ApplicationContext::moduleDisplayName(const nx::Uuid& id) const
{
    static const nx::Uuid kCloudPeerId("674BAFD7-4EEC-4BBA-84AA-A1BAEA7FC6DB");
    if (id == kCloudPeerId)
        return "Cloud";

    NX_MUTEX_LOCKER lock(&d->mutex);
    auto itr = d->longToShortInstanceId.find(id);
    return itr != d->longToShortInstanceId.end() ?
        QString::number(itr.value()) : id.toSimpleString();
}


QnStoragePluginFactory* ApplicationContext::storagePluginFactory() const
{
    return d->storagePluginFactory.get();
}

QnLongRunableCleanup* ApplicationContext::longRunableCleanup() const
{
    return d->longRunableCleanup.get();
}

ApplicationMetricsStorage* ApplicationContext::metrics() const
{
    return d->metricsStorage.get();
}

bool ApplicationContext::isCertificateValidationLevelStrict() const
{
    return false;
}

} // namespace nx::vms::common
