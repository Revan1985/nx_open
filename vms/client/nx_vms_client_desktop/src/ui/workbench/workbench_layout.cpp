// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout.h"

#include <limits>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtWidgets/QGraphicsWidget>

#include <client/client_runtime_settings.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource_helpers.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <utils/common/util.h>

#include "workbench_context.h"
#include "workbench_grid_walker.h"
#include "workbench_item.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_utility.h"

using namespace nx::vms::client::desktop;

using nx::vms::client::core::Geometry;
using nx::vms::client::core::LayoutResource;
using nx::vms::client::core::LayoutResourcePtr;

namespace {

void pointize(const QRect& rect, QSet<QPoint>* points)
{
    if (!points)
        return;

    for (int row = rect.top(); row <= rect.bottom(); row++)
    {
        for (int col = rect.left(); col <= rect.right(); col++)
            points->insert(QPoint(col, row));
    }
}

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCloudLayoutTheme = {{QnIcon::Normal, {.primary = "light10"}}};
nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSearchTheme = {{QnIcon::Normal, {.primary = "light16"}}};

NX_DECLARE_COLORIZED_ICON(kLayoutCloudIcon, "20x20/Solid/layout_cloud.svg", kCloudLayoutTheme)
NX_DECLARE_COLORIZED_ICON(kSearchIcon, "20x20/Outline/search.svg", kSearchTheme)

} // namespace

struct QnWorkbenchLayout::Private
{
    LayoutResourcePtr resource;

    std::unique_ptr<QnWorkbenchLayoutSynchronizer> synchronizer;

    /** Matrix map from coordinate to item. */
    QnMatrixMap<QnWorkbenchItem*> itemMap;

    /** Set of all items on this layout. */
    QSet<QnWorkbenchItem*> items;

    /** Map from item to its zoom target. */
    QHash<QnWorkbenchItem*, QnWorkbenchItem*> zoomTargetItemByItem;

    /** Map from zoom target item to its associated zoom items. */
    QMultiHash<QnWorkbenchItem*, QnWorkbenchItem*> itemsByZoomTargetItem;

    /** Set of item borders for fast bounding rect calculation. */
    QnRectSet rectSet;

    /** Current bounding rectangle. */
    QRect boundingRect;

    /** Map from resource to a set of items. */
    QHash<QnResourcePtr, QSet<QnWorkbenchItem*>> itemsByResource;

    /** Map from item's universally unique identifier to item. */
    QHash<nx::Uuid, QnWorkbenchItem*> itemByUuid;

    /** Empty item list, to return a reference to. */
    const QSet<QnWorkbenchItem*> noItems;

    QnLayoutFlags flags = QnLayoutFlag::Empty;

    QIcon icon;

    StreamSynchronizationState streamSynchronizationState;
};

QnWorkbenchLayout::QnWorkbenchLayout(
    WindowContext* windowContext,
    const LayoutResourcePtr& resource)
    :
    WindowContextAware(windowContext),
    d(new Private{.resource = resource})
{
    if (!NX_ASSERT(resource))
        return;

    if (resource->data().contains(Qn::LayoutFlagsRole))
        setFlags(flags() | resource->data(Qn::LayoutFlagsRole).value<QnLayoutFlags>());

    d->icon = calculateIcon();

    d->synchronizer = std::make_unique<QnWorkbenchLayoutSynchronizer>(this);
    d->synchronizer->update();

    connect(resource.get(), &QnResource::nameChanged, this, &QnWorkbenchLayout::titleChanged);
    connect(resource.get(), &LayoutResource::dataChanged, this, &QnWorkbenchLayout::dataChanged);
    connect(resource.get(), &QnLayoutResource::cellSpacingChanged,
        this, &QnWorkbenchLayout::cellSpacingChanged);
    connect(resource.get(), &QnLayoutResource::cellAspectRatioChanged,
        this, &QnWorkbenchLayout::cellAspectRatioChanged);

    connect(resource.get(), &QnLayoutResource::lockedChanged,
        this,
        [this]()
        {
            d->icon = calculateIcon();
            emit titleChanged();
        });

    connect(resource.get(), &LayoutResource::layoutAboutToBeSaved, d->synchronizer.get(),
        [this](const LayoutResourcePtr& layout)
        {
            if (NX_ASSERT(QThread::currentThread() == qApp->thread()))
                submit(layout);
        }, Qt::DirectConnection);

    connect(resource.get(), &LayoutResource::layoutRestored,
        d->synchronizer.get(), &QnWorkbenchLayoutSynchronizer::reset);

    connect(this, &QnWorkbenchLayout::dataChanged, this,
        [this](int role)
        {
            if (role == Qn::LayoutIconRole)
                emit titleChanged();
        });

    setStreamSynchronizationState(isPreviewSearchLayout()
        ? StreamSynchronizationState::disabled()
        : StreamSynchronizationState::live());
}

QnWorkbenchLayout::~QnWorkbenchLayout()
{
    // Synchronizer must be stopped here to submit pending items before destroying.
    d->synchronizer.reset();

    for (QnWorkbenchItem* item: d->items)
    {
        // Avoid unneeded `removeItem()` processing and signals sending.
        item->setLayout(nullptr);
        delete item;
    }
    d->items.clear();
}

QnLayoutFlags QnWorkbenchLayout::flags() const
{
    return d->flags;
}

QIcon QnWorkbenchLayout::icon() const
{
    return d->icon;
}

void QnWorkbenchLayout::setFlags(QnLayoutFlags value)
{
    if (d->flags == value)
        return;

    d->flags = value;
    emit flagsChanged();
}

nx::Uuid QnWorkbenchLayout::resourceId() const
{
    const auto& layout = resource();
    return layout ? layout->getId() : nx::Uuid();
}

LayoutResourcePtr QnWorkbenchLayout::resource() const
{
    return d->resource;
}

QnLayoutResource* QnWorkbenchLayout::resourcePtr() const
{
    return resource().data();
}

QnWorkbenchLayout* QnWorkbenchLayout::instance(const LayoutResourcePtr& resource)
{
    return appContext()->mainWindowContext()->workbench()->layout(resource);
}

QnWorkbenchLayout* QnWorkbenchLayout::instance(const QnVideoWallResourcePtr& videoWall)
{
    const auto& layouts =
        appContext()->mainWindowContext()->workbench()->layouts();
    for (const auto& layout: layouts)
    {
        if (layout->resource()->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>()
            == videoWall)
        {
            return layout.get();
        }
    }
    return nullptr;
}

QnWorkbenchLayoutSynchronizer* QnWorkbenchLayout::layoutSynchronizer() const
{
    return d->synchronizer.get();
}

QString QnWorkbenchLayout::name() const
{
    return d->resource->getName();
}

StreamSynchronizationState QnWorkbenchLayout::streamSynchronizationState() const
{
    return d->streamSynchronizationState;
}

void QnWorkbenchLayout::setStreamSynchronizationState(StreamSynchronizationState value)
{
    d->streamSynchronizationState = std::move(value);
}

bool QnWorkbenchLayout::update(const LayoutResourcePtr& resource)
{
    bool result = true;

    /* Unpin all items so that pinned state does not interfere with
     * incrementally moving the items. */
    for (auto item: d->items)
        item->setPinned(false);

    QSet<nx::Uuid> removed;

    for (auto item: items())
        removed.insert(item->uuid());

    for (const auto& data: resource->getItems())
    {
        auto item = this->item(data.uuid);
        if (!item)
        {
            if (const auto resource = nx::vms::client::core::getResourceByDescriptor(data.resource);
                resource
                    && ResourceAccessManager::hasPermissions(resource, Qn::ViewContentPermission))
            {
                auto workbenchItem = new QnWorkbenchItem(resource, data, this);
                // Each item must either be pinned or queued to be pinned.
                if (workbenchItem->flags() == 0)
                    workbenchItem->setFlags(Qn::ItemFlag::PendingGeometryAdjustment);
                addItem(workbenchItem);
            }
        }
        else if (ResourceAccessManager::hasPermissions(item->resource(), Qn::ViewContentPermission))
        {
            result &= item->update(data);
            removed.remove(item->uuid());
        }
    }

    for (const auto& uuid: removed)
        delete item(uuid);

    /* Update zoom targets. */
    for (const auto& data: resource->getItems())
    {
        auto item = this->item(data.uuid);
        auto currentZoomTargetItem = zoomTargetItem(item);
        auto expectedZoomTargetItem = this->item(data.zoomTargetUuid);
        if (currentZoomTargetItem != expectedZoomTargetItem)
            addZoomLink(item, expectedZoomTargetItem); //< Will automatically remove the old link if needed.
    }

    return result;
}

void QnWorkbenchLayout::submit(const LayoutResourcePtr& resource) const
{
    nx::vms::common::LayoutItemDataList datas;
    datas.reserve(items().size());
    for (auto item: items())
    {
        nx::vms::common::LayoutItemData data = item->data();
        data.zoomTargetUuid = this->zoomTargetUuidInternal(item);
        datas.push_back(data);
    }

    resource->setItems(datas);
}

void QnWorkbenchLayout::notifyTitleChanged()
{
    emit titleChanged();
}

bool QnWorkbenchLayout::canAutoAdjustAspectRatio()
{
    return items().size() == 1 && !resource()->hasBackground() &&  !hasCellAspectRatio();
}

void QnWorkbenchLayout::addItem(QnWorkbenchItem* item)
{
    NX_ASSERT(item && item->resource());
    if (!item)
        return;

    NX_ASSERT(!d->itemByUuid.contains(item->uuid()),
        "Item with UUID '%1' is already on layout '%2'.",
        item->uuid(), name());

    if (item->layout())
    {
        if (item->layout() == this)
            return;
        item->layout()->removeItem(item);
    }

    if (item->isPinned() && d->itemMap.isOccupied(item->geometry()))
        item->setFlag(Qn::Pinned, false);

    item->setLayout(this);
    d->items.insert(item);

    if (item->isPinned())
    {
        d->itemMap.fill(item->geometry(), item);
        NX_VERBOSE(this, nx::format("Add item to cell %1").arg(item->geometry()));
    }
    d->rectSet.insert(item->geometry());
    d->itemsByResource[item->resource()].insert(item);
    d->itemByUuid[item->uuid()] = item;

    emit itemAdded(item);

    updateBoundingRectInternal();
}

void QnWorkbenchLayout::removeItem(QnWorkbenchItem* item)
{
    if (!own(item))
        return;

    /* Remove all zoom links first. */
    if (auto targetItem = zoomTargetItem(item))
        removeZoomLink(item, targetItem);
    for (auto zoomItem: zoomItems(item))
        removeItem(zoomItem);

    /* Update internal data structures. */
    if (item->isPinned())
    {
        d->itemMap.clear(item->geometry());
        NX_VERBOSE(this, nx::format("Item removed from cell %1").arg(item->geometry()));
    }

    d->rectSet.remove(item->geometry());

    if (const auto itemsByResource = d->itemsByResource.find(item->resource());
        NX_ASSERT(itemsByResource != d->itemsByResource.end()))
    {
        itemsByResource->remove(item);
        if (itemsByResource->empty())
            d->itemsByResource.erase(itemsByResource);
    }

    d->itemByUuid.remove(item->uuid());

    if (NX_ASSERT(item->layout()))
    {
        auto layoutResource = item->layout()->resource();
        if (NX_ASSERT(layoutResource))
            layoutResource->cleanupItemData(item->uuid());
    }

    item->setLayout(nullptr);
    d->items.remove(item);
    emit itemRemoved(item);

    updateBoundingRectInternal();
}

void QnWorkbenchLayout::removeItems(const QnResourcePtr& resource)
{
    auto itemsByResourceIt = d->itemsByResource.find(resource);
    while (itemsByResourceIt != d->itemsByResource.end())
    {
        if (!NX_ASSERT(!itemsByResourceIt->empty()))
        {
            d->itemsByResource.erase(itemsByResourceIt);
            return;
        }

        removeItem(*itemsByResourceIt->begin());
        itemsByResourceIt = d->itemsByResource.find(resource);
    }
}

void QnWorkbenchLayout::addZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem)
{
    if (!own(item) || !own(zoomTargetItem))
        return;

    NX_ASSERT(item != zoomTargetItem, "Cannot create a loop zoom link.");
    if (item == zoomTargetItem)
        return;

    auto currentZoomTargetItem = item->zoomTargetItem();
    if (currentZoomTargetItem)
    {
        if (currentZoomTargetItem == zoomTargetItem)
            return;

        removeZoomLinkInternal(item, currentZoomTargetItem, false);
    }

    addZoomLinkInternal(item, zoomTargetItem, true);
}

void QnWorkbenchLayout::addZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
    bool notifyItem)
{
    d->zoomTargetItemByItem.insert(item, zoomTargetItem);
    d->itemsByZoomTargetItem.insert(zoomTargetItem, item);

    emit zoomLinkAdded(item, zoomTargetItem);
    if (notifyItem)
        emit item->zoomTargetItemChanged();
}

void QnWorkbenchLayout::removeZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem)
{
    if (!own(item) || !own(zoomTargetItem))
        return;

    NX_ASSERT(d->zoomTargetItemByItem.value(item) == zoomTargetItem,
        "Cannot remove a zoom link that does not exist in this layout.");

    if (d->zoomTargetItemByItem.value(item) != zoomTargetItem)
        return;

    removeZoomLinkInternal(item, zoomTargetItem, true);
}

void QnWorkbenchLayout::removeZoomLinkInternal(QnWorkbenchItem* item,
    QnWorkbenchItem* zoomTargetItem, bool notifyItem)
{
    d->zoomTargetItemByItem.remove(item);
    d->itemsByZoomTargetItem.remove(zoomTargetItem, item);

    emit zoomLinkRemoved(item, zoomTargetItem);
    if (notifyItem)
        emit item->zoomTargetItemChanged();
}

bool QnWorkbenchLayout::canMoveItem(QnWorkbenchItem* item, const QRect& geometry,
    Disposition* disposition)
{
    if (!own(item))
        return false;

    if (qnRuntime->isVideoWallMode())
        return true;

    if (item->isPinned())
    {
        return d->itemMap.isOccupiedBy(
            geometry,
            item,
            disposition ? &disposition->free : nullptr,
            disposition ? &disposition->occupied : nullptr
        );
    }

    pointize(geometry, disposition ? &disposition->free : nullptr);
    return true;
}

bool QnWorkbenchLayout::moveItem(QnWorkbenchItem* item, const QRect& geometry)
{
    if (!canMoveItem(item, geometry))
        return false;

    if (item->isPinned())
    {
        d->itemMap.clear(item->geometry());
        d->itemMap.fill(geometry, item);
        NX_VERBOSE(this, nx::format("Item moved from cell %1 to cell %2")
            .arg(item->geometry())
            .arg(geometry));
    }

    moveItemInternal(item, geometry);

    return true;
}

void QnWorkbenchLayout::moveItemInternal(QnWorkbenchItem* item, const QRect& geometry)
{
    d->rectSet.remove(item->geometry());
    d->rectSet.insert(geometry);

    updateBoundingRectInternal();

    item->setGeometryInternal(geometry);
}

bool QnWorkbenchLayout::canMoveItems(const QList<QnWorkbenchItem*>& items,
    const QList<QRect>& geometries, Disposition* disposition) const
{
    const bool returnEarly = (disposition == nullptr);
    NX_ASSERT(items.size() == geometries.size(), "Sizes of the given containers do not match.");

    if (items.size() != geometries.size())
        return false;

    if (items.empty())
        return true;

    /* Check whether it's our items. */
    using boost::algorithm::all_of;
    if (!all_of(items, [this](QnWorkbenchItem* item) { return own(item); }))
        return false;

    /* Good points are those where items can be moved.
     * Bad points are those where they cannot be moved. */
    QSet<QPoint> goodPointSet, badPointSet;

    /* Check whether new positions do not intersect each other. */
    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (!item->isPinned())
            continue;

        const QRect& geometry = geometries[i];
        for (int r = geometry.top(); r <= geometry.bottom(); r++)
        {
            for (int c = geometry.left(); c <= geometry.right(); c++)
            {
                QPoint point(c, r);

                bool conforms = !goodPointSet.contains(point);
                if (conforms)
                {
                    goodPointSet.insert(point);
                }
                else
                {
                    if (returnEarly)
                        return false;
                    badPointSet.insert(point);
                }
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    auto itemSet = QSet(items.cbegin(), items.cend());
    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (!item->isPinned())
        {
            pointize(geometries[i], &goodPointSet);
            continue;
        }

        bool conforms = d->itemMap.isOccupiedBy(
            geometries[i],
            itemSet,
            returnEarly ? nullptr : &goodPointSet,
            returnEarly ? nullptr : &badPointSet
        );

        if (returnEarly && !conforms)
            return false;
    }

    /* If we got here with early return on, then it means that everything is OK. */
    if (returnEarly)
        return true;

    goodPointSet.subtract(badPointSet);
    disposition->free = goodPointSet;
    disposition->occupied = badPointSet;

    return badPointSet.empty();
}

bool QnWorkbenchLayout::moveItems(const QList<QnWorkbenchItem*>& items,
    const QList<QRect>& geometries)
{
    if (!canMoveItems(items, geometries, nullptr))
        return false;

    /* Move. */
    for (auto item: items)
    {
        if (item->isPinned())
        {
            d->itemMap.clear(item->geometry());
            NX_VERBOSE(this, nx::format("Batch move items: clear cell %1").arg(item->geometry()));
        }
    }

    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (item->isPinned())
        {
            d->itemMap.fill(geometries[i], item);
            NX_VERBOSE(this, nx::format("Batch move items: put on cell %1").arg(geometries[i]));
        }
        moveItemInternal(item, geometries[i]);
    }

    emit itemsMoved(items);
    return true;
}

bool QnWorkbenchLayout::pinItem(QnWorkbenchItem* item, const QRect& geometry)
{
    if (!own(item))
        return false;

    if (item->isPinned())
        return moveItem(item, geometry);

    if (d->itemMap.isOccupied(geometry))
        return false;

    d->itemMap.fill(geometry, item);
    NX_VERBOSE(this, nx::format("Pin item to cell %1").arg(geometry));
    moveItemInternal(item, geometry);
    item->setFlagInternal(Qn::Pinned, true);
    return true;
}

bool QnWorkbenchLayout::unpinItem(QnWorkbenchItem* item)
{
    if (!own(item))
        return false;

    if (!item->isPinned())
        return true;

    d->itemMap.clear(item->geometry());
    NX_VERBOSE(this, nx::format("Unpin item from cell %1").arg(item->geometry()));
    item->setFlagInternal(Qn::Pinned, false);
    return true;
}

QnWorkbenchItem* QnWorkbenchLayout::item(const QPoint& position) const
{
    return d->itemMap.value(position, nullptr);
}

QnWorkbenchItem* QnWorkbenchLayout::item(const nx::Uuid& uuid) const
{
    return d->itemByUuid.value(uuid, nullptr);
}

QnWorkbenchItem* QnWorkbenchLayout::zoomTargetItem(QnWorkbenchItem* item) const
{
    return d->zoomTargetItemByItem.value(item, nullptr);
}

nx::Uuid QnWorkbenchLayout::zoomTargetUuidInternal(QnWorkbenchItem* item) const
{
    QnWorkbenchItem* zoomTargetItem = this->zoomTargetItem(item);
    return zoomTargetItem ? zoomTargetItem->uuid() : nx::Uuid();
}

QList<QnWorkbenchItem*> QnWorkbenchLayout::zoomItems(QnWorkbenchItem* zoomTargetItem) const
{
    return d->itemsByZoomTargetItem.values(zoomTargetItem);
}

bool QnWorkbenchLayout::isEmpty() const
{
    return d->items.isEmpty();
}

float QnWorkbenchLayout::cellAspectRatio() const
{
    return d->resource->cellAspectRatio();
}

bool QnWorkbenchLayout::hasCellAspectRatio() const
{
    return cellAspectRatio() > 0.0;
}

QSet<QnWorkbenchItem*> QnWorkbenchLayout::items(const QRect& region) const
{
    return d->itemMap.values(region);
}

QSet<QnWorkbenchItem*> QnWorkbenchLayout::items(const QList<QRect>& regions) const
{
    return d->itemMap.values(regions);
}

const QSet<QnWorkbenchItem*>& QnWorkbenchLayout::items(const QnResourcePtr& resource) const
{
    auto pos = d->itemsByResource.find(resource);
    return pos == d->itemsByResource.end() ? d->noItems : pos.value();
}

const QSet<QnWorkbenchItem*>& QnWorkbenchLayout::items() const
{
    return d->items;
}

QnResourceList QnWorkbenchLayout::itemResources() const
{
    return d->itemsByResource.keys();
}

bool QnWorkbenchLayout::isFreeSlot(const QPoint& gridPos) const
{
    return !d->itemMap.isOccupied(gridPos);
}

QRect QnWorkbenchLayout::closestFreeSlot(
    const QPointF& gridPos,
    const QSize& size,
    const TypedMagnitudeCalculator<QPoint>* const metric) const
{
    if (!metric)
    {
        // Use default metric if none provided.
        QnDistanceMagnitudeCalculator distanceMetric(
            gridPos - Geometry::toPoint(QSizeF(size)) / 2.0);
        return closestFreeSlot(gridPos, size, &distanceMetric);
    }

    NX_VERBOSE(this, "Seek for closestFreeSlot for item at %1 with size %2", gridPos, size);
    NX_VERBOSE(this, "Occupied cells are %1", d->itemMap.positions());

    // Grid cell where search starts.
    QPoint gridCell = (gridPos - Geometry::toPoint(QSizeF(size)) / 2.0).toPoint();
    NX_VERBOSE(this, "Start search from position %1", gridCell);

    // Current bests.
    qreal bestDistance = std::numeric_limits<qreal>::max();
    QPoint bestDelta = QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());

    static constexpr int kNoEdge = 0;
    Qt::Edges allEdges = (Qt::TopEdge | Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge);

    // Border being walked.
    int processingEdge = kNoEdge;

    // Borders that are known to not contain positions closer to the target than the current best.
    int checkedEdges = 0;

    // The algorithm works in the following way:
    // * The walker checks all the available cells.
    // * Expand the walker to the best possible edge.
    // * Repeat until we check all the edges at least once AND do not improve current result.
    QnWorkbenchGridWalker walker;
    while (true)
    {
        NX_VERBOSE(this, "Walking over rect %1", walker.rect());
        if (walker.hasNext())
        {
            QPoint delta = walker.next();
            qreal distance = metric->calculate(gridCell + delta);
            NX_VERBOSE(this, "Pos %1 (delta %2), distance %3", gridCell + delta, delta, distance);

            if (distance > bestDistance || qFuzzyEquals(distance, bestDistance))
                continue;

            // We have improved result a bit, do not exclude this edge next time.
            NX_VERBOSE(this, "Result was improved, keep the %1", Qt::Edge(processingEdge));
            processingEdge = kNoEdge;

            if (d->itemMap.isOccupied(QRect(gridCell + delta, size)))
            {
                NX_VERBOSE(this, "Pos %1 (delta %2) is occupied", gridCell + delta, delta);
                continue;
            }

            checkedEdges = 0;
            bestDistance = distance;
            bestDelta = delta;
            NX_VERBOSE(this, "Remember delta %1 as best (distance %2)", delta, distance);
        }
        else
        {
            if (processingEdge == kNoEdge)
                NX_VERBOSE(this, "Walker has finished the edge, which was kept");
            else
                NX_VERBOSE(this, "Walker has finished the %1", Qt::Edge(processingEdge));
            checkedEdges |= processingEdge;
            if (checkedEdges == allEdges && bestDistance < std::numeric_limits<qreal>::max())
            {
                NX_VERBOSE(this, "Best position found: %1 (delta %2, distance %3)",
                    QRect(gridCell + bestDelta, size),
                    bestDelta,
                    bestDistance);
                return QRect(gridCell + bestDelta, size);
            }

            // We have checked all edges but there were no place at all. Let's start over.
            if (checkedEdges == allEdges)
                checkedEdges = kNoEdge;

            using edge_t = std::pair<Qt::Edge, QPoint>;
            std::array<edge_t, 4> expansion{{
                {Qt::RightEdge, gridCell + QPoint(walker.rect().right() + 1, 0)},
                {Qt::LeftEdge, gridCell + QPoint(walker.rect().left() - 1, 0)},
                {Qt::BottomEdge, gridCell + QPoint(0, walker.rect().bottom() + 1)},
                {Qt::TopEdge, gridCell + QPoint(0, walker.rect().top() - 1)},
            }};

            std::list<edge_t> edgesToCheck;
            for (auto v: expansion)
            {
                // Rely on the monotonic magnitude nature, do not expand to the same side again
                if ((checkedEdges & v.first) == v.first)
                    continue;

                NX_VERBOSE(this, "Checking edge %1: pos %2, distance %3",
                    v.first,
                    v.second,
                    metric->calculate(v.second));
                edgesToCheck.push_back(v);
            }

            if (!NX_ASSERT(!edgesToCheck.empty()))
            {
                // Something goes wrong. Let's start over.
                checkedEdges = kNoEdge;
                continue;
            }

            const auto bestEdgeIter = std::min_element(edgesToCheck.cbegin(), edgesToCheck.cend(),
                [metric, &gridCell](const edge_t& left, const edge_t& right)
                {
                    return metric->calculate(left.second) < metric->calculate(right.second);
                });

            const Qt::Edge bestEdge = bestEdgeIter->first;
            NX_VERBOSE(this, "Expanding the best border %1", bestEdge);
            walker.expand(bestEdge);
            processingEdge = bestEdge;
        }
    }
}

void QnWorkbenchLayout::updateBoundingRectInternal()
{
    QRect boundingRect = d->rectSet.boundingRect();
    if (d->boundingRect == boundingRect)
        return;

    QRect oldRect = d->boundingRect;
    d->boundingRect = boundingRect;
    emit boundingRectChanged(oldRect, boundingRect);
}

qreal QnWorkbenchLayout::cellSpacing() const
{
    return d->resource->cellSpacing();
}

bool QnWorkbenchLayout::locked() const
{
    return d->resource->locked();
}

const QRect& QnWorkbenchLayout::boundingRect() const
{
    return d->boundingRect;
}

bool QnWorkbenchLayout::own(QnWorkbenchItem* item) const
{
    NX_ASSERT(item);
    NX_ASSERT(item->layout() == this, "Item must belong to this layout.");
    return item && item->layout() == this;
}

QIcon QnWorkbenchLayout::calculateIcon() const
{
    if (d->resource->hasFlags(Qn::cross_system))
        return qnSkin->icon(kLayoutCloudIcon);

    if (isPreviewSearchLayout())
        return qnSkin->icon(kSearchIcon);

    return d->resource->data(Qn::LayoutIconRole).value<QIcon>();
}

QVariant QnWorkbenchLayout::data(int role) const
{
    return d->resource->data(role);
}

void QnWorkbenchLayout::setData(int role, const QVariant& value)
{
    d->resource->setData(role, value);
}

void QnWorkbenchLayout::centralizeItems()
{
    QRect brect = boundingRect();
    int xdiff = -brect.center().x();
    int ydiff = -brect.center().y();

    QList<QnWorkbenchItem*> itemsList = d->items.values();
    QList<QRect> geometries;
    foreach(QnWorkbenchItem* item, itemsList)
        geometries << item->geometry().adjusted(xdiff, ydiff, xdiff, ydiff);
    moveItems(itemsList, geometries);
}

bool QnWorkbenchLayout::isPreviewSearchLayout() const
{
    return ::isPreviewSearchLayout(resource());
}

bool QnWorkbenchLayout::isShowreelReviewLayout() const
{
    return ::isShowreelReviewLayout(resource());
}

bool QnWorkbenchLayout::isVideoWallReviewLayout() const
{
    return ::isVideoWallReviewLayout(resource());
}

QString QnWorkbenchLayout::toString() const
{
    return nx::format("QnWorkbenchLayout %1 (%2)").args(name(), resource());
}
