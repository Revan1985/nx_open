// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_api_backend.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qjson.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>

#include "helpers.h"
#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {
namespace {

Error resourceNotFound()
{
    return Error::invalidArguments(ResourcesApiBackend::tr("Resource not found"));
}

} // namespace

ResourcesApiBackend::ResourcesApiBackend(QObject* parent):
    base_type(parent)
{
    const auto addResources =
        [this](const QnResourceList& resources)
        {
            const auto addedResources = resources.filtered(
                [](const QnResourcePtr& resource)
                {
                    return isResourceAvailable(resource);
                });

            for (const auto& resource: addedResources)
                emit added(Resource::from(resource));
        };

    const auto removeResources =
        [this](const QnResourceList& resources)
        {
            // Do not check availability since after removal we don't have any permission
            // for deleted resource.
            for (const auto& resource: resources)
                emit removed(ResourceUniqueId::from(resource));
        };

    const auto pool = appContext()->unifiedResourcePool();
    connect(pool, &core::UnifiedResourcePool::resourcesAdded, this, addResources);
    connect(pool, &core::UnifiedResourcePool::resourcesRemoved, this, removeResources);
}

ResourcesApiBackend::~ResourcesApiBackend()
{
}

QList<Resource> ResourcesApiBackend::resources() const
{
    const auto resources = appContext()->unifiedResourcePool()->resources(
        [](const QnResourcePtr& resource)
        {
            return isResourceAvailable(resource);
        });
    return Resource::from(resources);
}

ResourceResult ResourcesApiBackend::resource(const ResourceUniqueId& resourceId) const
{
    const auto resource = getResourceIfAvailable(resourceId);
    return resource
        ? ResourceResult{Error::success(), Resource::from(resource)}
        : ResourceResult{
            Error::failed(tr("Resource is not available for the usage with JS API")), {}};
}

ParameterResult ResourcesApiBackend::parameter(
    const ResourceUniqueId& resourceId,
    const QString& name) const
{
    const QnResourcePtr resource = getResourceIfAvailable(resourceId);
    if (!resource)
        return {resourceNotFound()};

    const QString value = resource->getProperty(name);
    if (value.isNull())
        return {Error::invalidArguments(tr("Parameter not found"))};

    if (auto [result, ok] = nx::reflect::json::deserialize<QJsonValue>(value.toStdString()); ok)
        return {Error::success(), result};

    return {Error::success(), value};
}

ParameterResult ResourcesApiBackend::parameterNames(const ResourceUniqueId& resourceId) const
{
    const QnResourcePtr resource = getResourceIfAvailable(resourceId);
    if (!resource)
        return {resourceNotFound()};

    const QSet<QString> names = resource->systemContext()->resourcePropertyDictionary()
        ->allPropertyNamesByResource()[resource->getId()];

    return {Error::success(), QJsonArray::fromStringList({names.begin(), names.end()})};
}

Error ResourcesApiBackend::setParameter(
    const ResourceUniqueId& resourceId,
    const QString& name,
    const QJsonValue& value)
{
    const QnResourcePtr resource = getResourceIfAvailable(resourceId);
    if (!resource)
        return resourceNotFound();

    if (!ResourceAccessManager::hasPermissions(resource, Qn::ReadWriteSavePermission))
        return Error::denied();

    resource->setProperty(name, QString::fromStdString(nx::reflect::json::serialize(value)));
    return resource->systemContext()->resourcePropertyDictionary()->saveParams({resource->getId()})
        ? Error::success()
        : Error::failed();
}

} // namespace nx::vms::client::desktop::jsapi::detail
