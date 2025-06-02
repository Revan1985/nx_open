// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::core {

/**
 * Unified interface to access all available Resource Pools. Listens for System Contexts creation
 * and destruction. Emits resourcesAdded and resourcesRemoved when a Context is added or removed
 * correspondingly.
 */
class NX_VMS_CLIENT_CORE_API UnifiedResourcePool: public QObject
{
    Q_OBJECT

public:
    UnifiedResourcePool(QObject* parent = nullptr);

    using ResourceFilter = nx::vms::common::ResourceFilter;
    QnResourceList resources(ResourceFilter filter = {}) const;

    /** Find all resources with given Id. */
    QnResourceList resources(const nx::Uuid& resourceId) const;

    QnResourcePtr resource(const nx::Uuid& resourceId, const nx::Uuid& localSystemId) const;

signals:
    /**
     * Emitted whenever any new Resource is added to any of the Resource Pools.
     */
    void resourcesAdded(const QnResourceList& resources);

    /**
     * Emitted whenever any resource is removed from the pool.
     */
    void resourcesRemoved(const QnResourceList& resources);
};

} // namespace nx::vms::client::core
