// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

#include "system_context_aware.h" //< Forward declarations.

Q_MOC_INCLUDE("camera/camera_bookmarks_manager.h")
Q_MOC_INCLUDE("nx/vms/client/core/watchers/feature_access_watcher.h")
Q_MOC_INCLUDE("nx/vms/client/core/watchers/user_watcher.h")
Q_MOC_INCLUDE("nx/vms/client/core/watchers/watermark_watcher.h")

class QnPtzControllerPool;
class QQmlContext;
class QnCameraBookmarksManager;
class QnServerStorageManager;

namespace ec2 {

class AbstractECConnection;
using AbstractECConnectionPtr = std::shared_ptr<AbstractECConnection>;

} // namespace ec2

namespace nx::network::http { class Credentials; }
namespace nx::vms::api { struct SystemSettings; }
namespace nx::vms::rules { class Engine; }

namespace nx::vms::client::core {

class AccessController;
class AnalyticsEventsSearchTreeBuilder;
class CameraDataManager;
class FeatureAccessWatcher;
class IoPortsCompatibilityInterface;
class ServerRuntimeEventConnector;
class UserWatcher;
class VideoCache;
class RemoteSessionTimeoutWatcher;
class WatermarkWatcher;
class TrafficRelayUrlWatcher;

namespace analytics {
class AttributeHelper;
} // namespace analytics

class NX_VMS_CLIENT_CORE_API SystemContext: public common::SystemContext
{
    Q_OBJECT
    using base_type = common::SystemContext;

    Q_PROPERTY(analytics::TaxonomyManager* taxonomyManager
        READ taxonomyManager
        CONSTANT)

    Q_PROPERTY(WatermarkWatcher* watermarkWatcher
        READ watermarkWatcher
        CONSTANT)

    Q_PROPERTY(UserWatcher* userWatcher
        READ userWatcher
        CONSTANT)

    Q_PROPERTY(QnCameraBookmarksManager* bookmarksManager
        READ cameraBookmarksManager
        CONSTANT)

    Q_PROPERTY(FeatureAccessWatcher* featureAccess
        READ featureAccess
        CONSTANT)

public:
    /**
     * Initialize client-core-level System Context based on existing common-level System Context.
     * Destruction order must be handled by the caller.
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     */
    SystemContext(Mode mode, nx::Uuid peerId, QObject* parent = nullptr);
    virtual ~SystemContext() override;

    static SystemContext* fromResource(const QnResourcePtr& resource);

    /**
     * Information about the Server we are connected to.
     */
    nx::vms::api::ModuleInformation moduleInformation() const;

    /**
     * Update remote session this Context belongs to. Current client architecture supposes one main
     * System Context to exist through whole Application lifetime, and different Sessions are
     * loaded into it and unloaded when session is terminated.
     * // TODO: #sivanov Invert architecture, so Remote Session will own the System Context.
     */
    void setSession(std::shared_ptr<RemoteSession> session);

    /**
     * Set connection which this Context should use to communicate with the corresponding System.
     * Connection is mutually exclusive with ::setSession() and should be used for session-less
     * Contexts only.
     */
    void setConnection(RemoteConnectionPtr connection);

    /**
     * Id of the server which was used to establish the Remote Session (if it is present).
     */
    nx::Uuid currentServerId() const;

    /**
     * The server which was used to establish the Remote Session (if it is present).
     */
    QnMediaServerResourcePtr currentServer() const;

    /**
     * Remote session this context belongs to (if any).
     */
    std::shared_ptr<RemoteSession> session() const;

    /**
     * Connection which this Context should use to communicate with the corresponding System.
     */
    RemoteConnectionPtr connection() const;

    /**
     * Local id of the system to which we are currently connected.
     */
    nx::Uuid localSystemId() const;

    /**
     * Cloud id of the system (if any) to which we are currently connected.
     */
    QString cloudSystemId() const;

    /**
     * Credentials we are using to authorize the connection.
     */
    virtual nx::network::http::Credentials credentials() const override;

    /** API interface of the currently connected server. */
    rest::ServerConnectionPtr connectedServerApi() const;

    /**
     * Id of the connection in the Audit Trail and Runtime Info Manager (taken from the remote
     * connection if it is established).
     */
    virtual nx::Uuid auditId() const override;

    /**
     * Established p2p connection (if any).
     */
    ec2::AbstractECConnectionPtr messageBusConnection() const;

    /** Message processor, cast to actual class. */
    QnClientMessageProcessor* clientMessageProcessor() const;

    QnPtzControllerPool* ptzControllerPool() const;

    UserWatcher* userWatcher() const;
    QnUserResourcePtr user() const;

    WatermarkWatcher* watermarkWatcher() const;

    ServerTimeWatcher* serverTimeWatcher() const;

    QnCameraBookmarksManager* cameraBookmarksManager() const;

    FeatureAccessWatcher* featureAccess() const;

    CameraDataManager* cameraDataManager() const;

    nx::vms::api::SystemSettings* systemSettings() const;

    virtual nx::vms::common::SessionTokenHelperPtr getSessionTokenHelper() const;

    analytics::TaxonomyManager* taxonomyManager() const;

    analytics::AttributeHelper* analyticsAttributeHelper() const;

    QnServerStorageManager* serverStorageManager() const;

    ServerRuntimeEventConnector* serverRuntimeEventConnector() const;

    AccessController* accessController() const;

    virtual void startModuleDiscovery(
        nx::vms::discovery::Manager* moduleDiscoveryManager) override;

    VideoCache* videoCache() const;

    AnalyticsEventsSearchTreeBuilder* analyticsEventsSearchTreeBuilder() const;

    IoPortsCompatibilityInterface* ioPortsInterface() const;

    RemoteSessionTimeoutWatcher* sessionTimeoutWatcher() const;

    TrafficRelayUrlWatcher* trafficRelayUrlWatcher() const;

signals:
    void remoteIdChanged(const nx::Uuid& id);

    /**
     * Signal which sent when current user of this system context is changed. Actual only while
     * legacy system context architecture is in action, when there is only one active system
     * context, and new remote connection clears everything in it and fills it again.
     */
    void userChanged(const QnUserResourcePtr& user);

protected:
    virtual void setMessageProcessor(QnCommonMessageProcessor* messageProcessor) override;
    void resetAccessController(AccessController* accessController);

protected:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
