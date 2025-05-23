// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_manager.h"

#include <client/client_globals.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

Qn::Permissions calculateNonContextualResourcePermissions(const QnResourcePtr& resource)
{
    // TODO: #sivanov Code duplication.
    if (auto layout = resource.dynamicCast<core::LayoutResource>())
    {
        // Some layouts are created with predefined permissions which are never changed.
        QVariant presetPermissions = layout->data().value(Qn::LayoutPermissionsRole);
        if (presetPermissions.isValid() && presetPermissions.canConvert<int>())
            return static_cast<Qn::Permissions>(presetPermissions.toInt()) | Qn::ReadPermission;
    }

    return Qn::NoPermissions;
}

} // namespace

Qn::Permissions ResourceAccessManager::permissions(const QnResourcePtr& resource)
{
    if (!NX_ASSERT(resource))
        return Qn::NoPermissions;

    const auto systemContext = SystemContext::fromResource(resource);
    if (!systemContext)
        return calculateNonContextualResourcePermissions(resource);

    const auto accessController = systemContext->accessController();
    if (!NX_ASSERT(accessController))
        return Qn::NoPermissions;

    return accessController->permissions(resource);
}

bool ResourceAccessManager::hasPermissions(
    const QnResourcePtr& resource,
    Qn::Permissions requiredPermissions)
{
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

nx::vms::client::core::AccessController* ResourceAccessManager::accessController(
    const QnResourcePtr& resource)
{
    if (!NX_ASSERT(resource))
        return nullptr;

    const auto systemContext = SystemContext::fromResource(resource);
    return systemContext
        ? systemContext->accessController()
        : nullptr;
}

} // namespace nx::vms::client::desktop
