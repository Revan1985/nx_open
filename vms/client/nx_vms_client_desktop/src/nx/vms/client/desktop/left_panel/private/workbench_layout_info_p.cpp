// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_info_p.h"

#include <client/client_runtime_settings.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

class WorkbenchLayoutInfo::ResourceContainer: public AbstractResourceContainer
{
    WorkbenchLayoutInfo* const q;

public:
    ResourceContainer(WorkbenchLayoutInfo* q):
        AbstractResourceContainer(q),
        q(q)
    {
    }

    virtual bool containsResource(QnResource* resource) const override
    {
        return resource
            && !q->workbench()->currentLayout()->items(resource->toSharedPointer()).empty();
    }
};

WorkbenchLayoutInfo::WorkbenchLayoutInfo(WindowContext* context, QObject* parent):
    QObject(parent),
    WindowContextAware(context),
    m_resources(new ResourceContainer(this))
{
    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        [this]()
        {
             workbench()->currentLayoutResource()->disconnect(this);
             m_controlledVideoWall = {};
        });

    connect(workbench()->currentLayoutResource().get(), &QnLayoutResource::lockedChanged,
        this, &WorkbenchLayoutInfo::isLockedChanged);

    connect(workbench(), &Workbench::currentLayoutChanged, this,
        [this]()
        {
            connect(workbench()->currentLayoutResource().get(), &QnLayoutResource::lockedChanged,
                this, &WorkbenchLayoutInfo::isLockedChanged);

            emit currentLayoutChanged();
            emit itemCountChanged();
            emit resourcesChanged();
            emit isLockedChanged();
        });

    connect(workbench(), &Workbench::currentLayoutItemsChanged, this,
        [this]()
        {
            emit itemCountChanged();
            emit resourcesChanged();
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &WorkbenchLayoutInfo::currentResourceChanged);
}

QnLayoutResource* WorkbenchLayoutInfo::currentLayout() const
{
    return workbench()->currentLayoutResource().get();
}

QnResource* WorkbenchLayoutInfo::currentResource() const
{
    return navigator()->currentResource().get();
}

AbstractResourceContainer* WorkbenchLayoutInfo::resources() const
{
    return m_resources;
}

nx::Uuid WorkbenchLayoutInfo::reviewedShowreelId() const
{
    return workbench()->currentLayout()->data(Qn::ShowreelUuidRole).value<nx::Uuid>();
}

QnVideoWallResource* WorkbenchLayoutInfo::reviewedVideoWall() const
{
    return workbench()->currentLayout()->data(Qn::VideoWallResourceRole)
        .value<QnVideoWallResourcePtr>().get();
}

QnVideoWallResource* WorkbenchLayoutInfo::controlledVideoWall() const
{
    if (m_controlledVideoWall.has_value())
        return m_controlledVideoWall->get();

    const auto itemId = controlledVideoWallItemId();
    if (itemId.isNull())
    {
        m_controlledVideoWall = QnVideoWallResourcePtr();
        return nullptr;
    }

    m_controlledVideoWall = system()->resourcePool()->getVideoWallItemByUuid(itemId).videowall();
    return m_controlledVideoWall->get();
}

nx::Uuid WorkbenchLayoutInfo::controlledVideoWallItemId() const
{
    return workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
}

int WorkbenchLayoutInfo::itemCount() const
{
    return workbench()->currentLayout()->items().size();
}

int WorkbenchLayoutInfo::maximumItemCount() const
{
    return qnRuntime->maxSceneItems();
}

bool WorkbenchLayoutInfo::isLocked() const
{
    return workbench()->currentLayout()->locked();
}

bool WorkbenchLayoutInfo::isPreviewSearchLayout() const
{
    return workbench()->currentLayout()->isPreviewSearchLayout();
}

bool WorkbenchLayoutInfo::isShowreelReviewLayout() const
{
    return workbench()->currentLayout()->isShowreelReviewLayout();
}

} // namespace nx::vms::client::desktop
