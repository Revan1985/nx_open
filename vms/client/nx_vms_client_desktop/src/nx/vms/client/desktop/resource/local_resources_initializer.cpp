// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_resources_initializer.h"

#include <QtCore/QFileInfo>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>

namespace nx::vms::client::desktop {

LocalResourcesInitializer::LocalResourcesInitializer(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this, &LocalResourcesInitializer::onResourcesAdded);

    onResourcesAdded(resourcePool()->getResources());
}

void LocalResourcesInitializer::onResourcesAdded(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        // Quick check.
        if (!resource->hasFlags(Qn::layout))
            continue;

        QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
        if (!NX_ASSERT(layout))
            continue;

        for (auto& data: layout->getItems())
        {
            if (core::isCrossSystemResource(data.resource)
                || core::isDesktopCameraResource(data.resource))
            {
                continue;
            }

            QnResourcePtr resource = core::getResourceByDescriptor(data.resource);

            if (!resource
                && !data.resource.path.isEmpty()
                && QFileInfo::exists(data.resource.path))
            {
                /* Try to load local resource. */
                resource = QnResourcePtr(new QnAviResource(data.resource.path));
                resourcePool()->addResource(resource);
            }

            if (resource)
            {
                data.resource = core::descriptor(resource);
                layout->updateItem(data);
            }
            else
            {
                layout->removeItem(data.uuid);
            }
        }
    }
}

} // namespace nx::vms::client::desktop
