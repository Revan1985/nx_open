// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CameraResourceIndex;
class UserLayoutResourceIndex;
class UserRolesProvider;
class WebPageResourceIndex;

class NX_VMS_CLIENT_DESKTOP_API ResourceTreeItemKeySourcePool: public SystemContextAware
{
    using UniqueResourceSourcePtr = entity_item_model::UniqueResourceSourcePtr;
    using UniqueUuidSourcePtr = entity_item_model::UniqueUuidSourcePtr;
    using UniqueStringSourcePtr = entity_item_model::UniqueStringSourcePtr;

public:
    ResourceTreeItemKeySourcePool(
        SystemContext* systemContext,
        const CameraResourceIndex* cameraResourceIndex,
        const UserLayoutResourceIndex* sharedLayoutResourceIndex);

    ~ResourceTreeItemKeySourcePool();

    /**
     * Provides non-fake servers in the system that the given access subject has access to, as
     * well as parent servers for resources the given access subjects has access to.
     * @param reduceEdgeServers If true, EDGE servers will be substituted by corresponding
     *     camera resources.
     */
    UniqueResourceSourcePtr serversSource(
        const QnResourceAccessSubject& accessSubject,
        bool reduceEdgeServers);

    /**
     * Provides non-fake servers in the system that the given access subject has access to.
     */
    UniqueResourceSourcePtr healthMonitorsSource(const QnResourceAccessSubject& accessSubject);

    using ResourceFilter = nx::vms::common::ResourceFilter;

    /**
     * Provides all device resources stored in the resource pool filtered with specified filter.
     */
    UniqueResourceSourcePtr allDevicesSource(
        const QnResourceAccessSubject& accessSubject,
        const ResourceFilter& resourceFilter);

    /**
     * Provides all device resources stored in the resource pool.
     */
    UniqueResourceSourcePtr devicesSource(
        const QnResourceAccessSubject& accessSubject,
        const QnMediaServerResourcePtr& parentServer,
        const ResourceFilter& resourceFilter);

    /**
     * Provides device resources stored in the resource pool along with proxied web pages.
     * @todo Mixing devices with web pages better be done little later.
     * @param parentServer If not null only single devices that belong to that server are
     *     provided, whole set of single devices stored in the resource pool otherwise.
     */
    UniqueResourceSourcePtr devicesAndProxiedWebPagesSource(
        const QnMediaServerResourcePtr& parentServer,
        const QnResourceAccessSubject& accessSubject);

    /**
     * Provides all layout resources accessible by user including intercom and cloud layouts.
     */
    UniqueResourceSourcePtr layoutsSource(const QnUserResourcePtr& user);

    /**
     * Provides all layout resources accessible by user except intercom and cloud layouts.
     */
    UniqueResourceSourcePtr allLayoutsSource(const QnUserResourcePtr& user);

    UniqueResourceSourcePtr shareableLayoutsSource(
        const QnUserResourcePtr& user,
        const ResourceFilter& resourceFilter = ResourceFilter());

    /**
     * Provides integration resources stored in the resource pool.
     */
    UniqueResourceSourcePtr integrationsSource(
        const QnResourceAccessSubject& subject,
        bool includeProxiedIntegrations);

    /**
     * Provides web page resources stored in the resource pool.
     */
    UniqueResourceSourcePtr webPagesSource(
        const QnResourceAccessSubject& subject,
        bool includeProxiedWebPages);

    /**
     * Provides web page resources stored in the resource pool.
     */
    UniqueResourceSourcePtr webPagesAndIntegrationsSource(
        const QnResourceAccessSubject& subject,
        bool includeProxied);

    /**
     * Provides all video walls resources stored in the resource pool.
     */
    UniqueResourceSourcePtr videoWallsSource(const QnResourceAccessSubject& subject);

    /**
     * Provides all local file layout resources stored in the resource pool.
     */
    UniqueResourceSourcePtr fileLayoutsSource();

    /**
     * Provides all local media resources stored in the resource pool.
     */
    UniqueResourceSourcePtr localMediaSource();

    /**
     * Provides other server ids which are stored in the other servers manager.
     */
    UniqueUuidSourcePtr otherServersSource();

    /**
     * Provides ids of cloud systems retrieved from the CloudSystemFinder instance.
     */
    UniqueStringSourcePtr cloudSystemsSource();

    /**
     * Provides cameras of the cloud system.
     */
    UniqueResourceSourcePtr cloudSystemCamerasSource(const QString& systemId);

private:
    const CameraResourceIndex* m_cameraResourceIndex;
    const UserLayoutResourceIndex* m_userLayoutResourceIndex;
    QScopedPointer<WebPageResourceIndex> m_webPageResourceIndex;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
