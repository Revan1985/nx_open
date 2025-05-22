// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_descriptor_helpers.h"

#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::core {

namespace {

static const QString kCloudScheme = "cloud://";

static const QString kDesktopCameraMark = "desktopCamera://";

QString resourcePath(const nx::Uuid& resourceId, const QString& cloudSystemId)
{
    if (NX_ASSERT(!cloudSystemId.isEmpty()))
        return nx::format(kCloudScheme + "%1.%2", cloudSystemId, resourceId.toSimpleString());

    return {};
}

QString resourcePath(const QnResourcePtr& resource, bool forceCloud)
{
    if (resource->hasFlags(Qn::exported_layout))
        return resource->getUrl();

    if (resource->hasFlags(Qn::local_media))
        return resource->getUrl();

    if (resource->hasFlags(Qn::web_page))
        return resource->getUrl();

    if (resource.dynamicCast<QnLayoutResource>() && resource->hasFlags(Qn::cross_system))
        return resourcePath(resource->getId(), genericCloudSystemId());

    if (const auto camera = resource.dynamicCast<core::CrossSystemCameraResource>())
        return resourcePath(camera->getId(), camera->systemId());

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        camera && camera->hasFlags(Qn::desktop_camera))
    {
        return kDesktopCameraMark + camera->getPhysicalId();
    }

    const auto systemContext = SystemContext::fromResource(resource);
    const bool belongsToOtherContext = (systemContext != appContext()->currentSystemContext());
    if (NX_ASSERT(systemContext) && (forceCloud || belongsToOtherContext))
    {
        // TODO: #sivanov Update cloudSystemId in the current system context module information
        // when the system is bound to the cloud or vise versa.

        // Using module information is generally better for the remote cloud systems, but we cannot
        // use it for the current system as it may be bound to the cloud in the current session.
        const QString cloudSystemId = belongsToOtherContext
            ? systemContext->moduleInformation().cloudSystemId
            : systemContext->globalSettings()->cloudSystemId();

        if (NX_ASSERT(!cloudSystemId.isEmpty()))
        {
            return resourcePath(resource->getId(), cloudSystemId);
        }
    }

    return {};
}

} // namespace

nx::vms::common::ResourceDescriptor descriptor(const QnResourcePtr& resource, bool forceCloud)
{
    if (!NX_ASSERT(resource))
        return {};

    return {
        .id=resource->getId(),
        .path=resourcePath(resource, forceCloud),
        .name=resource->getName()
    };
}

/** Find Resource in a corresponding System Context. */
QnResourcePtr getResourceByDescriptor(const nx::vms::common::ResourceDescriptor& descriptor)
{
    SystemContext* systemContext = nullptr;
    if (isCrossSystemResource(descriptor))
    {
        const QString cloudSystemId = crossSystemResourceSystemId(descriptor);
        systemContext = appContext()->systemContextByCloudSystemId(cloudSystemId);
    }

    if (!systemContext)
        systemContext = appContext()->currentSystemContext();

    if (!NX_ASSERT(systemContext))
        return {};

    const auto resource = systemContext->resourcePool()->getResourceByDescriptor(descriptor);

    if (resource)
        return resource;

    // There are resources that are not cross-system resources but exist together with cloud
    // layouts. Therefore, it is necessary to check for the presence of such resources in cloud
    // layout system context resource pool. For example web pages and local files.
    systemContext = appContext()->systemContextByCloudSystemId(genericCloudSystemId());
    if (systemContext)
        return systemContext->resourcePool()->getResourceByDescriptor(descriptor);

    return {};
}

bool isCrossSystemResource(const nx::vms::common::ResourceDescriptor& descriptor)
{
    const QString systemId = crossSystemResourceSystemId(descriptor);
    return !systemId.isEmpty()
        && systemId != appContext()->currentSystemContext()->moduleInformation().cloudSystemId;
}

QString crossSystemResourceSystemId(const nx::vms::common::ResourceDescriptor& descriptor)
{
    if (!descriptor.path.startsWith(kCloudScheme))
        return {};

    return descriptor.path.mid(
        kCloudScheme.length(),
        descriptor.path.indexOf('.') - kCloudScheme.length());
}

bool isDesktopCameraResource(const nx::vms::common::ResourceDescriptor& descriptor)
{
    return descriptor.path.startsWith(kDesktopCameraMark);
}

QString getDesktopCameraPhysicalId(const nx::vms::common::ResourceDescriptor& descriptor)
{
    if (!descriptor.path.startsWith(kDesktopCameraMark))
        return {};

    return descriptor.path.mid(kDesktopCameraMark.size());
}

QString genericCloudSystemId()
{
    static const QString kGenericCloudSystemId = nx::Uuid().toSimpleString();
    return kGenericCloudSystemId;
}

} // namespace nx::vms::client::core
