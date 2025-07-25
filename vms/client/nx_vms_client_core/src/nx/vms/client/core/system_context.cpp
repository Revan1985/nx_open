// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QtQml>

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_access_controller.h>
#include <nx/vms/client/core/cross_system/cross_system_ptz_controller_pool.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/rules/client_router.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/utils/video_cache.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>
#include <nx/vms/rules/engine_holder.h>
#include <nx/vms/rules/initializer.h>

#include "private/system_context_data_p.h"

namespace nx::vms::client::core {

namespace {

static const QString kSystemContextPropertyName("systemContext");

} // namespace

SystemContext::SystemContext(Mode mode, nx::Uuid peerId, QObject* parent):
    base_type(mode, peerId, parent),
    d(new Private{.q=this})
{
    d->serverTimeWatcher = std::make_unique<ServerTimeWatcher>(this);
    std::unique_ptr<QnServerStorageManager> serverStorageManager;

    d->analyticsAttributeHelper = std::make_unique<
        analytics::AttributeHelper>(analyticsTaxonomyStateWatcher());

    switch (mode)
    {
        case Mode::client:
        {
            d->serverRuntimeEventConnector = std::make_unique<ServerRuntimeEventConnector>();
            d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
            d->cameraDataManager = std::make_unique<CameraDataManager>(this);
            d->userWatcher = std::make_unique<UserWatcher>(this);
            d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
            d->serverStorageManager = std::make_unique<QnServerStorageManager>(this);
            d->vmsRulesEngineHolder = std::make_unique<nx::vms::rules::EngineHolder>(
                this,
                std::make_unique<core::rules::ClientRouter>(this),
                std::make_unique<nx::vms::rules::Initializer>(this),
                /*separateThread*/ false);
            d->taxonomyManager = std::make_unique<analytics::TaxonomyManager>(this);
            d->videoCache = std::make_unique<VideoCache>(this);
            d->sessionTimeoutWatcher =
                std::make_unique<RemoteSessionTimeoutWatcher>(globalSettings());
            d->trafficRelayUrlWatcher = std::make_unique<TrafficRelayUrlWatcher>(this);
            d->featureAccessWatcher = std::make_unique<FeatureAccessWatcher>(this);
            break;
        }

        case Mode::crossSystem:
        {
            d->videoCache = std::make_unique<VideoCache>(this);
            d->userWatcher = std::make_unique<UserWatcher>(this);
            d->cameraBookmarksManager = std::make_unique<QnCameraBookmarksManager>(this);
            d->cameraDataManager = std::make_unique<CameraDataManager>(this);
            d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
            d->featureAccessWatcher = std::make_unique<FeatureAccessWatcher>(this);
            if (ini().allowCslObjectSearch)
                d->taxonomyManager = std::make_unique<analytics::TaxonomyManager>(this);
            break;
        }

        case Mode::cloudLayouts:
        case Mode::unitTests:
            break;

        case Mode::server:
            NX_CRITICAL(mode != Mode::server, "Client context cannot be of server type");
            break;
    }

    if (mode == Mode::crossSystem)
        resetAccessController(new CrossSystemAccessController(this));
    else
        resetAccessController(new AccessController(this));

    if (d->userWatcher)
    {
        connect(d->userWatcher.get(), &UserWatcher::userChanged, this,
            [this](const QnUserResourcePtr& user)
            {
                if (d->accessController)
                    d->accessController->setUser(user);
                emit userChanged(user);
            });
    }
}

SystemContext::~SystemContext()
{
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

nx::vms::api::ModuleInformation SystemContext::moduleInformation() const
{
    if (auto connection = this->connection())
        return connection->moduleInformation();

    return {};
}

void SystemContext::setSession(std::shared_ptr<RemoteSession> session)
{
    const auto oldConnection = connection();

    if (d->sessionTimeoutWatcher)
        d->sessionTimeoutWatcher->sessionStopped();

    {
        NX_MUTEX_LOCKER lock(&d->sessionMutex);
        NX_ASSERT(!d->connection);

        // Make sure existing session will be terminated outside of the mutex.
        std::swap(d->session, session);
    }
    if (d->session && d->sessionTimeoutWatcher)
        d->sessionTimeoutWatcher->sessionStarted(d->session);

    d->initializeIoPortsInterface();

    d->handleConnectionChanged(oldConnection, connection());

    emit remoteIdChanged(currentServerId());
}

void SystemContext::setConnection(RemoteConnectionPtr value)
{
    const auto oldConnection = connection();
    NX_ASSERT(!d->connection);
    NX_ASSERT(!d->session);

    d->connection = value;

    d->initializeIoPortsInterface();

    d->handleConnectionChanged(oldConnection, connection());
}

nx::Uuid SystemContext::currentServerId() const
{
    return moduleInformation().id;
}

QnMediaServerResourcePtr SystemContext::currentServer() const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(currentServerId());
}

std::shared_ptr<RemoteSession> SystemContext::session() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    return d->session;
}

RemoteConnectionPtr SystemContext::connection() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    if (d->session)
        return d->session->connection();

    return d->connection;
}

nx::Uuid SystemContext::localSystemId() const
{
    const auto& currentConnection = connection();
    return currentConnection ? currentConnection->moduleInformation().localSystemId : nx::Uuid();
}

QString SystemContext::cloudSystemId() const
{
    const auto& currentConnection = connection();
    return currentConnection ? currentConnection->moduleInformation().cloudSystemId : QString{};
}

nx::network::http::Credentials SystemContext::credentials() const
{
    if (auto connection = this->connection())
        return connection->credentials();

    return {};
}

rest::ServerConnectionPtr SystemContext::connectedServerApi() const
{
    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

nx::Uuid SystemContext::auditId() const
{
    if (auto connection = this->connection())
        return connection->auditId();

    return {};
}

ec2::AbstractECConnectionPtr SystemContext::messageBusConnection() const
{
    if (auto connection = this->connection())
        return connection->messageBusConnection();

    return {};
}

QnClientMessageProcessor* SystemContext::clientMessageProcessor() const
{
    return static_cast<QnClientMessageProcessor*>(this->messageProcessor());
}

QnPtzControllerPool* SystemContext::ptzControllerPool() const
{
    if (!NX_ASSERT(qApp->thread() == QThread::currentThread()))
        return d->ptzControllerPool.get();

    if (!d->ptzControllerPool)
    {
        SystemContext* context = const_cast<SystemContext*>(this);
        if (mode() == Mode::crossSystem)
            d->ptzControllerPool = std::make_unique<CrossSystemPtzControllerPool>(context);
        else
            d->ptzControllerPool = std::make_unique<ptz::ControllerPool>(context);
    }

    return d->ptzControllerPool.get();
}

UserWatcher* SystemContext::userWatcher() const
{
    return d->userWatcher.get();
}

QnUserResourcePtr SystemContext::user() const
{
    return userWatcher()->user();
}

WatermarkWatcher* SystemContext::watermarkWatcher() const
{
    return d->watermarkWatcher.get();
}

ServerTimeWatcher* SystemContext::serverTimeWatcher() const
{
    return d->serverTimeWatcher.get();
}

common::SessionTokenHelperPtr SystemContext::getSessionTokenHelper() const
{
    return nullptr;
}

QnCameraBookmarksManager* SystemContext::cameraBookmarksManager() const
{
    return d->cameraBookmarksManager.get();
}

FeatureAccessWatcher* SystemContext::featureAccess() const
{
    return d->featureAccessWatcher.get();
}

analytics::TaxonomyManager* SystemContext::taxonomyManager() const
{
    QQmlEngine::setObjectOwnership(d->taxonomyManager.get(), QQmlEngine::CppOwnership);
    return d->taxonomyManager.get();
}

analytics::AttributeHelper* SystemContext::analyticsAttributeHelper() const
{
    return d->analyticsAttributeHelper.get();
}

QnServerStorageManager* SystemContext::serverStorageManager() const
{
    return d->serverStorageManager.get();
}

ServerRuntimeEventConnector* SystemContext::serverRuntimeEventConnector() const
{
    return d->serverRuntimeEventConnector.get();
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = qobject_cast<QnClientMessageProcessor*>(messageProcessor);
    if (!NX_ASSERT(clientMessageProcessor, "Invalid message processor type"))
        return;

    if (mode() == Mode::client)
    {
        d->serverRuntimeEventConnector->setMessageProcessor(clientMessageProcessor);
        d->analyticsEventsSearchTreeBuilder =
            std::make_unique<AnalyticsEventsSearchTreeBuilder>(this);
    }

    if (!vmsRulesEngine())
        return;

    nx::vms::rules::EngineHolder::connectEngine(
        vmsRulesEngine(),
        clientMessageProcessor,
        Qt::QueuedConnection);
}

AccessController* SystemContext::accessController() const
{
    return d->accessController.get();
}

VideoCache* SystemContext::videoCache() const
{
    return d->videoCache.get();
}

AnalyticsEventsSearchTreeBuilder* SystemContext::analyticsEventsSearchTreeBuilder() const
{
    return d->analyticsEventsSearchTreeBuilder.get();
}

IoPortsCompatibilityInterface* SystemContext::ioPortsInterface() const
{
    return d->ioPortsInterface.get();
}

RemoteSessionTimeoutWatcher* SystemContext::sessionTimeoutWatcher() const
{
    return d->sessionTimeoutWatcher.get();
}

TrafficRelayUrlWatcher* SystemContext::trafficRelayUrlWatcher() const
{
    return d->trafficRelayUrlWatcher.get();
}

CameraDataManager* SystemContext::cameraDataManager() const
{
    return d->cameraDataManager.get();
}

void SystemContext::resetAccessController(AccessController* accessController)
{
    d->accessController.reset(accessController);
}

void SystemContext::startModuleDiscovery(
    nx::vms::discovery::Manager* moduleDiscoveryManager)
{
    base_type::startModuleDiscovery(moduleDiscoveryManager);
    d->serverPrimaryInterfaceWatcher = std::make_unique<ServerPrimaryInterfaceWatcher>(this);
}

} // namespace nx::vms::client::core
