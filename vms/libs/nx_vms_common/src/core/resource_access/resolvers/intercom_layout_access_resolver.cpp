// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_layout_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/intercom/utils.h>

namespace {

bool isIntercom(const QnVirtualCameraResourcePtr& camera)
{
    return camera->isIntercom();
};

} // namespace

namespace nx::core::access {

using namespace nx::vms::api;
using namespace nx::vms::common;

class IntercomLayoutAccessResolver::Private: public QObject
{
    IntercomLayoutAccessResolver* const q;

public:
    explicit Private(IntercomLayoutAccessResolver* q,
        AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool);

    ResourceAccessMap ensureAccessMap(const nx::Uuid& subjectId) const;

    void handleBaseAccessChanged(const QSet<nx::Uuid>& subjectIds);
    void handleReset();

    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
    void handleIntercomsAdded(const QnVirtualCameraResourceList& intercoms);

    void invalidateSubjects(const QSet<nx::Uuid>& subjectIds);

    QSet<nx::Uuid> invalidateCache(); //< Returns all subject ids that were cached.

    void notifyResolutionChanged(const QnResourceList& intercoms,
        const QSet<nx::Uuid>& knownAffectedSubjectIds);

public:
    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<QnResourcePool> resourcePool;
    QSet<nx::Uuid> allIntercoms;
    mutable QHash<nx::Uuid, ResourceAccessMap> cachedAccessMaps;
    mutable nx::Mutex mutex;
};

// -----------------------------------------------------------------------------------------------
// IntercomLayoutAccessResolver

IntercomLayoutAccessResolver::IntercomLayoutAccessResolver(
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, resourcePool))
{
}

IntercomLayoutAccessResolver::~IntercomLayoutAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap IntercomLayoutAccessResolver::resourceAccessMap(const nx::Uuid& subjectId) const
{
    return d->baseResolver && d->resourcePool
        ? d->ensureAccessMap(subjectId)
        : ResourceAccessMap();
}

GlobalPermissions IntercomLayoutAccessResolver::globalPermissions(const nx::Uuid& subjectId) const
{
    return d->baseResolver
        ? d->baseResolver->globalPermissions(subjectId)
        : GlobalPermissions{};
}

ResourceAccessDetails IntercomLayoutAccessResolver::accessDetails(
    const nx::Uuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    if (!d->baseResolver || !d->resourcePool || !NX_ASSERT(resource))
        return {};

    const bool hasAccess = accessRights(subjectId, resource).testFlag(accessRight);
    if (!hasAccess)
        return {};

    QnResourcePtr target = resource;
    if (const auto layout = resource.objectCast<QnLayoutResource>())
    {
        if (const auto parentResource = layout->getParentResource())
        {
            if (nx::vms::common::isIntercomOnIntercomLayout(parentResource, layout))
                target = parentResource;
        }
    }

    return d->baseResolver->accessDetails(subjectId, target, accessRight);
}

// -----------------------------------------------------------------------------------------------
// IntercomLayoutAccessResolver::Private

IntercomLayoutAccessResolver::Private::Private(
    IntercomLayoutAccessResolver* q,
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool)
    :
    q(q),
    baseResolver(baseResolver),
    resourcePool(resourcePool)
{
    NX_CRITICAL(baseResolver && resourcePool);

    connect(q->notifier(), &Notifier::subjectsSubscribed,
        baseResolver->notifier(), &Notifier::subscribeSubjects, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsReleased,
        baseResolver->notifier(), &Notifier::releaseSubjects, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessChanged,
        this, &Private::handleBaseAccessChanged, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessReset,
        this, &Private::handleReset, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        this, &Private::handleResourcesAdded, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        this, &Private::handleResourcesRemoved, Qt::DirectConnection);

    const auto intercoms = resourcePool->getResources<QnVirtualCameraResource>(&isIntercom);

    NX_MUTEX_LOCKER lk(&mutex);
    for (const auto& intercom: intercoms)
        allIntercoms.insert(intercom->getId());
}

ResourceAccessMap IntercomLayoutAccessResolver::Private::ensureAccessMap(
    const nx::Uuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&mutex);

    if (cachedAccessMaps.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    ResourceAccessMap accessMap = baseResolver->resourceAccessMap(subjectId);

    constexpr AccessRights kRequiredAccessRights = AccessRight::view | AccessRight::userInput;
    constexpr AccessRights kGrantedAccessRights = AccessRight::view;

    for (QnResourcePtr intercom: resourcePool->getResourcesByIds(allIntercoms))
    {
        const auto accessRights = baseResolver->accessRights(subjectId, intercom);
        const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercom);
        if (accessRights.testFlags(kRequiredAccessRights))
            accessMap.emplace(intercomLayoutId, kGrantedAccessRights);
        else
            accessMap.remove(intercomLayoutId);
    }

    cachedAccessMaps.emplace(subjectId, accessMap);
    baseResolver->notifier()->subscribeSubjects({subjectId});

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, toString(accessMap, resourcePool));

    return accessMap;
}

void IntercomLayoutAccessResolver::Private::handleBaseAccessChanged(const QSet<nx::Uuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    invalidateSubjects(subjectIds);
    q->notifyAccessChanged(subjectIds);
}

void IntercomLayoutAccessResolver::Private::invalidateSubjects(const QSet<nx::Uuid>& subjectIds)
{
    QSet<nx::Uuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: subjectIds)
        {
            if (cachedAccessMaps.remove(subjectId))
                affectedCachedSubjectIds.insert(subjectId);
        }
    }

    if (affectedCachedSubjectIds.empty())
        return;

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
}

void IntercomLayoutAccessResolver::Private::handleReset()
{
    const auto affectedCachedSubjectIds = invalidateCache();
    NX_DEBUG(q, "Base resolution reset, whole cache invalidated");
    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    q->notifyAccessReset();
}

QSet<nx::Uuid> IntercomLayoutAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);
    const auto cachedSubjectIds = QSet<nx::Uuid>(
        cachedAccessMaps.keyBegin(), cachedAccessMaps.keyEnd());

    cachedAccessMaps.clear();
    return cachedSubjectIds;
}

void IntercomLayoutAccessResolver::Private::handleResourcesAdded(const QnResourceList& resources)
{
    QnVirtualCameraResourceList intercoms;

    for (const auto& resource: resources)
    {
        const auto camera = resource.objectCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

        if (camera->isIntercom())
        {
            intercoms.push_back(camera);
        }
        else
        {
            connect(camera.get(), &QnVirtualCameraResource::ioPortDescriptionsChanged, this,
                [this](const auto& securityCamera)
                {
                    // TODO: #skolesnik Remove objectCast when `QnVirtualCameraResource` is used.
                    const auto camera = securityCamera.template objectCast<QnVirtualCameraResource>();
                    if (camera && camera->isIntercom() && !allIntercoms.contains(camera->getId()))
                    {
                        handleIntercomsAdded({camera});
                        camera->disconnect(this);
                    }
                });
        }
    }

    handleIntercomsAdded(intercoms);
}

void IntercomLayoutAccessResolver::Private::handleResourcesRemoved(const QnResourceList& resources)
{
    if (!baseResolver || allIntercoms.empty())
        return;

    QnResourceList removedIntercoms;
    QSet<nx::Uuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);
        for (const auto& resource: resources)
        {
            if (!allIntercoms.remove(resource->getId()))
                continue;

            for (const auto& subjectId: nx::utils::keyRange(cachedAccessMaps))
            {
                if (baseResolver->accessRights(subjectId, resource))
                    affectedCachedSubjectIds.insert(subjectId);
            }

            for (const auto& subjectId: affectedCachedSubjectIds)
                cachedAccessMaps.remove(subjectId);

            removedIntercoms.push_back(resource);
        }
    }

    NX_DEBUG(q, "Intercoms %1 removed, %2", removedIntercoms,
        affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(removedIntercoms, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void IntercomLayoutAccessResolver::Private::handleIntercomsAdded(
    const QnVirtualCameraResourceList& intercoms)
{
    if (!baseResolver || intercoms.empty())
        return;

    QSet<nx::Uuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);
        for (const auto& intercom: intercoms)
        {
            for (const auto& subjectId: nx::utils::keyRange(cachedAccessMaps))
            {
                if (baseResolver->accessRights(subjectId, intercom))
                    affectedCachedSubjectIds.insert(subjectId);
            }

            for (const auto& subjectId: affectedCachedSubjectIds)
                cachedAccessMaps.remove(subjectId);

            allIntercoms.insert(intercom->getId());
        }
    }

    NX_DEBUG(q, "Intercoms %1 added, %3", intercoms,
        affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(intercoms, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void IntercomLayoutAccessResolver::Private::notifyResolutionChanged(
    const QnResourceList& intercoms, const QSet<nx::Uuid>& knownAffectedSubjectIds)
{
    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<nx::Uuid> affectedWatchedSubjectIds;

    for (const auto id: watchedSubjectIds)
    {
        if (knownAffectedSubjectIds.contains(id))
        {
            affectedWatchedSubjectIds.insert(id);
        }
        else
        {
            for (const auto& intercom: intercoms)
            {
                if (baseResolver->accessRights(id, intercom))
                {
                    affectedWatchedSubjectIds.insert(id);
                    break;
                }
            }
        }
    }

    q->notifyAccessChanged(affectedWatchedSubjectIds);
}

} // namespace nx::core::access
