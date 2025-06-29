// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_drag_drop_decorator_model.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <ui/workbench/handlers/workbench_action_handler.h>

namespace {

using namespace nx::vms::client::core;

using NodeTypeSet = QSet<nx::vms::client::desktop::ResourceTree::NodeType>;

static const QString kPureTreeResourcesOnlyMimeType =
    "application/x-pure-tree-resources-only";

bool mimeFormatsIntersects(const QStringList& firstMimeFormats,
    const QStringList& secondMimeFormats)
{
    return nx::utils::toQSet(firstMimeFormats).intersects(nx::utils::toQSet(secondMimeFormats));
}

QModelIndex topLevelParent(const QModelIndex& index)
{
    if (!index.isValid())
        return {};

    QModelIndex result = index;
    while (result.parent().isValid())
        result = result.parent();

    return result;
}

bool hasNodeType(
    const QModelIndex& index,
    nx::vms::client::desktop::ResourceTree::NodeType nodeType)
{
    if (!index.isValid())
        return false;

    QVariant nodeTypeData = index.data(Qn::NodeTypeRole);
    if (!NX_ASSERT(!nodeTypeData.isNull()))
        return false;

    return nodeTypeData.value<nx::vms::client::desktop::ResourceTree::NodeType>() == nodeType;
}

bool hasTopLevelNodeType(
    const QModelIndex& index,
    nx::vms::client::desktop::ResourceTree::NodeType nodeType)
{
    return hasNodeType(topLevelParent(index), nodeType);
}

bool hasResourceFlags(const QModelIndex& index, Qn::ResourceFlags resourceFlags)
{
    if (!index.isValid())
        return false;

    const auto resource = index.data(ResourceRole).value<QnResourcePtr>();
    return resource && resource->hasFlags(resourceFlags);
}

bool isWebPage(const QnResourcePtr& resource)
{
    if (const auto webPageResource = resource.dynamicCast<QnWebPageResource>())
        return webPageResource->subtype() == nx::vms::api::WebPageSubtype::none;
    return false;
}

bool isIntegration(const QnResourcePtr& resource)
{
    if (const auto webPageResource = resource.dynamicCast<QnWebPageResource>())
        return webPageResource->subtype() == nx::vms::api::WebPageSubtype::clientApi;
    return false;
}

bool mimeDataResourcesMatches(
    const nx::vms::client::desktop::MimeData& mimeData,
    std::function<bool(const QnResourcePtr&)> predicate)
{
    const auto resources = mimeData.resources();
    return std::all_of(resources.begin(), resources.end(), predicate);
}

QnMediaServerResourcePtr parentServer(const QModelIndex& index)
{
    if (!index.isValid())
        return QnMediaServerResourcePtr();

    auto parent = index.parent();
    while (parent.isValid())
    {
        if (auto server = parent.data(ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>())
            return server;
        parent = parent.parent();
    }

    return QnMediaServerResourcePtr();
}

} // namespace

namespace nx::vms::client::desktop {

ResourceTreeDragDropDecoratorModel::ResourceTreeDragDropDecoratorModel(
    QnResourcePool* resourcePool,
    menu::Manager* actionManager,
    ui::workbench::ActionHandler* actionHandler)
    :
    base_type(nullptr),
    m_resourcePool(resourcePool),
    m_actionManager(actionManager),
    m_actionHandler(actionHandler)
{
    NX_ASSERT(resourcePool && actionManager);
}

ResourceTreeDragDropDecoratorModel::~ResourceTreeDragDropDecoratorModel()
{
}

Qt::ItemFlags ResourceTreeDragDropDecoratorModel::flags(const QModelIndex& index) const
{
    auto result = base_type::flags(index);

    if (!index.isValid())
        return result;

    const auto targetIndex = targetDropIndex(index);
    result.setFlag(Qt::ItemIsDropEnabled, targetIndex.isValid()
        ? base_type::flags(targetIndex).testFlag(Qt::ItemIsDropEnabled)
        : false);

    return result;
}

QStringList ResourceTreeDragDropDecoratorModel::mimeTypes() const
{
    if (!sourceModel())
        return QStringList();

    auto result = MimeData::mimeTypes();
    result.append(kPureTreeResourcesOnlyMimeType);
    return result;
}

QMimeData* ResourceTreeDragDropDecoratorModel::mimeData(const QModelIndexList& indexes) const
{
    using NodeType = ResourceTree::NodeType;

    if (!sourceModel())
        return nullptr;

    QScopedPointer<QMimeData> baseMimeData(base_type::mimeData(indexes));
    if (!baseMimeData)
        return nullptr;

    // This flag services the assertion that cameras can be dropped on server
    // if and only if they are dragged from tree (not from layouts).
    bool pureTreeResourcesOnly = true;

    QSet<nx::Uuid> entities;
    QSet<QnResourcePtr> resources;
    menu::Parameters::ArgumentHash arguments;

    NodeTypeSet topLevelParentNodeTypes;

    for (const auto& index: indexes)
    {
        if (!index.isValid())
            continue;

        if (auto resource = index.data(core::ResourceRole).value<QnResourcePtr>())
            resources.insert(resource);

        const auto nodeTypeData = index.data(Qn::NodeTypeRole);
        const auto topLevelParentNodeTypeData = topLevelParent(index).data(Qn::NodeTypeRole);
        if (!NX_ASSERT(!nodeTypeData.isNull()))
            continue;

        const auto nodeType = nodeTypeData.value<NodeType>();
        if (!topLevelParentNodeTypeData.isNull())
        {
            topLevelParentNodeTypes.insert(
                topLevelParentNodeTypeData.value<ResourceTree::NodeType>());
        }

        switch (nodeType)
        {
            case NodeType::recorder:
            {
                const QAbstractItemModel* model = index.model();
                for (int row = 0; row < model->rowCount(index); ++row)
                {
                    const auto childIndex = model->index(row, 0, index);
                    if (auto resource = childIndex.data(core::ResourceRole).value<QnResourcePtr>())
                        resources.insert(resource);
                }
                break;
            }
            case NodeType::customResourceGroup:
            {
                const QAbstractItemModel* model = index.model();

                auto childResourceIndexes = model->match(
                    index.model()->index(0, 0, index),
                    Qn::NodeTypeRole,
                    QVariant::fromValue<NodeType>(NodeType::resource),
                    -1,
                    {Qt::MatchRecursive, Qt::MatchExactly});

                const auto customGroupId = index.data(Qn::ResourceTreeCustomGroupIdRole);
                if (!customGroupId.isNull())
                    arguments.insert(Qn::ResourceTreeCustomGroupIdRole, customGroupId);

                for (const auto& resourceIndex: childResourceIndexes)
                {
                    if (auto resource = resourceIndex.data(core::ResourceRole).value<QnResourcePtr>())
                        resources.insert(resource);
                }
                break;
            }
            case NodeType::videoWallItem:
            case NodeType::showreel:
                entities.insert(index.data(core::UuidRole).value<nx::Uuid>());
                break;

            case NodeType::layoutItem:
                pureTreeResourcesOnly = false;
                break;

            default:
                break;
        }
    }

    arguments.insert(Qn::TopLevelParentNodeTypeRole, QVariant::fromValue(topLevelParentNodeTypes));

    MimeData data(baseMimeData.get());
    data.setEntities(entities.values());
    data.setResources(resources.values());
    data.setArguments(arguments);
    data.setData(kPureTreeResourcesOnlyMimeType, QByteArray(pureTreeResourcesOnly ? "1" : "0"));
    data.addContextInformation(appContext()->mainWindowContext()); //< TODO: #mmalofeev use actual window context.

    return data.createMimeData();
}

bool ResourceTreeDragDropDecoratorModel::canDropMimeData(const QMimeData* mimeData,
    Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    using NodeType = ResourceTree::NodeType;

    if (!sourceModel())
        return false;

    if (!mimeData)
        return false;

    // Check if the action is supported.
    if (!supportedDropActions().testFlag(action))
        return false;

    // Check if the format is supported.
    if (!mimeFormatsIntersects(mimeData->formats(), MimeData::mimeTypes()))
        return sourceModel()->canDropMimeData(mimeData, action, row, column, parent);

    // Check if the drop is targeted for the scene.
    MimeData data(mimeData);

    // Check whether mime data is made by the same user from the same system.
    if (!data.allowedInWindowContext(appContext()->mainWindowContext())) //< TODO: #mmalofeev use actual window context.
        return false;

    if (data.arguments().contains(Qn::ItemTimeRole))
        return false;

    const auto targetIndex = targetDropIndex(parent);
    if (!targetIndex.isValid())
        return false;

    // Do not allow to drop anything than web pages and integrations into corresponding groups.
    if (ini().webPagesAndIntegrations) //< Separate "Web Pages" and "Integrations" top level items.
    {
        if (hasTopLevelNodeType(targetIndex, NodeType::webPages))
            return mimeDataResourcesMatches(data, isWebPage);

        if (hasTopLevelNodeType(targetIndex, NodeType::integrations))
            return mimeDataResourcesMatches(data, isIntegration);
    }
    else //< Both Web Pages and Integrations are grouped by "Web Pages" parent item.
    {
        if (hasTopLevelNodeType(targetIndex, NodeType::webPages))
        {
            return mimeDataResourcesMatches(data,
                [](const QnResourcePtr& resource)
                {
                    return isWebPage(resource) || isIntegration(resource);
                });
        }
    }

    // Don't allow to drop camera items from layouts to the main devices tree.
    if (hasResourceFlags(targetIndex, Qn::server)
        || hasNodeType(targetIndex, NodeType::camerasAndDevices)
        || hasNodeType(targetIndex, NodeType::customResourceGroup))
    {
        return mimeData->data(kPureTreeResourcesOnlyMimeType) == QByteArray("1");
    }

    return true;
}

// TODO: #vbreus It's a spaghetti function, well-cooked however. Split to action-specific parts.
bool ResourceTreeDragDropDecoratorModel::dropMimeData(const QMimeData* mimeData,
    Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    using namespace menu;
    using NodeType = ResourceTree::NodeType;

    if (!sourceModel())
        return false;

    if (!canDropMimeData(mimeData, action, row, column, parent))
        return false;

    // Resource tree drop is working only for resources that are already in the pool.
    MimeData data(mimeData);
    resourcePool()->addNewResources(data.resources());

    const auto index = targetDropIndex(parent);

    QSet<ResourceTree::NodeType> topLevelParentNodeTypes =
        data.arguments().value(Qn::TopLevelParentNodeTypeRole).value<QSet<ResourceTree::NodeType>>();

    // Drop on videowall is handled by videowall.
    if (hasNodeType(index, NodeType::videoWallItem))
    {
        const auto videoWallItems = resourcePool()->getVideoWallItemsByUuid(data.entities());

        Parameters parameters =
            videoWallItems.empty() ? Parameters(data.resources()) : Parameters(videoWallItems);

        parameters.setArgument(Qn::VideoWallItemGuidRole,
            index.data(core::UuidRole).value<nx::Uuid>());

        actionManager()->trigger(DropOnVideoWallItemAction, parameters);

        return true;
    }

    // TODO: #vbreus Get clear specification regarding un-proxying web pages by drag-and-drop
    // action. Make sure that custom group manipulating doesn't conflict with this mechanic.
    else if (hasNodeType(index, NodeType::webPages) && !ini().webPagesAndIntegrations)
    {
        // Setting empty parent ID for web pages forces them not to be proxied.
        const auto webPages = data.resources().filtered<QnWebPageResource>();

        m_actionHandler->moveResourcesToServer(webPages, /*server*/ {},
            [this](QnResourceList moved)
            {
                // Group ID is discarded for web pages being moved back to the 'Web Pages' parent
                // node.
                moveCustomGroup(moved, {}, {});
            });

        return true;
    }

    if (hasNodeType(index, NodeType::camerasAndDevices))
    {
        const auto cameras = data.resources().filtered<QnVirtualCameraResource>();

        QnResourceList sourceResources;
        std::copy(cameras.cbegin(), cameras.cend(), std::back_inserter(sourceResources));

        const auto webPages = data.resources().filtered<QnWebPageResource>();
        std::copy(webPages.cbegin(), webPages.cend(), std::back_inserter(sourceResources));

        if (sourceResources.empty())
            return true;

        const auto dragGroupId =
            data.arguments().value(Qn::ResourceTreeCustomGroupIdRole).toString();

        if (dragGroupId.isEmpty())
        {
            actionManager()->trigger(AssignCustomGroupIdAction, Parameters(sourceResources)
                .withArgument(Qn::ResourceTreeCustomGroupIdRole, QString()));
        }
        else
        {
            actionManager()->trigger(MoveToCustomGroupAction, Parameters(sourceResources)
                .withArgument(Qn::ResourceTreeCustomGroupIdRole, dragGroupId)
                .withArgument(Qn::TargetResourceTreeCustomGroupIdRole, QString()));
        }
    }

    if (hasNodeType(index, NodeType::customResourceGroup))
    {
        QnVirtualCameraResourceList cameras;
        QnLayoutResourceList layouts;

        // Parent server as displayed, i.e it will be null if server nodes aren't displayed in the
        // resource tree.
        const auto dragGroupId =
            data.arguments().value(Qn::ResourceTreeCustomGroupIdRole).toString();

        const auto dropParentServer = parentServer(index);
        const auto dropGroupId = index.data(Qn::ResourceTreeCustomGroupIdRole).toString();

        // Dropping cameras or web pages into group within "Layouts" node has no effect.
        if (hasTopLevelNodeType(index, NodeType::layouts))
            layouts = data.resources().filtered<QnLayoutResource>();

        // Dropping layouts or web pages into group within "Cameras & Devices" or server node has
        // no effect.
        if (hasTopLevelNodeType(index, NodeType::camerasAndDevices)
            || hasTopLevelNodeType(index, NodeType::servers)
            || hasResourceFlags(topLevelParent(index), Qn::server))
        {
            cameras = data.resources().filtered<QnVirtualCameraResource>();
        }

        moveResources(
            cameras, /*webPages*/ {}, layouts, dragGroupId, dropGroupId, dropParentServer);

        return true;
    }

    if (hasNodeType(index, NodeType::layouts))
    {
        const auto layouts = data.resources().filtered<QnLayoutResource>();

        const auto dragGroupId =
            data.arguments().value(Qn::ResourceTreeCustomGroupIdRole).toString();

        moveResources({}, {}, layouts, dragGroupId, QString(), {});
        return true;
    }

    // Drop resources that can be displayed at the scene to the layout.
    if (hasResourceFlags(index, Qn::layout))
    {
        const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();
        const auto layout = resource.staticCast<LayoutResource>();

        const auto droppable = data.resources()
            .filtered([](const QnResourcePtr& resource)
            {
                if (!QnResourceAccessFilter::isOpenableInLayout(resource))
                    return false;

                const auto accessController =
                    SystemContext::fromResource(resource)->accessController();
                return accessController->hasPermissions(resource, Qn::ViewContentPermission);
            });
        if (droppable.isEmpty())
            return true;

        actionManager()->trigger(OpenInLayoutAction,
            Parameters(droppable).withArgument(core::LayoutResourceRole, layout));
        actionManager()->trigger(SaveLayoutAction, layout);

        return true;
    }

    // Drop device or web page item on the server allows to move devices or proxied web pages
    // between servers.
    if (hasResourceFlags(index, Qn::server))
    {
        const auto server = index.data(core::ResourceRole).value<QnResourcePtr>()
            .dynamicCast<QnMediaServerResource>();

        if (!NX_ASSERT(!server.isNull()))
            return false;

        const auto cameras = data.resources().filtered<QnVirtualCameraResource>();
        const auto webPages = data.resources().filtered<QnWebPageResource>();

        const auto dragGroupId =
            data.arguments().value(Qn::ResourceTreeCustomGroupIdRole).toString();

        const auto dropGroupId = index.data(Qn::ResourceTreeCustomGroupIdRole).toString();

        moveResources(cameras, webPages, /*layouts*/ {}, dragGroupId, dropGroupId, server);
        return true;
    }

    return true;
}

void ResourceTreeDragDropDecoratorModel::moveCustomGroup(
    QnResourceList sourceResources,
    const QString& dragGroupId,
    const QString& dropGroupId)
{
    if (sourceResources.isEmpty())
        return;

    using namespace menu;

    if (dragGroupId.isEmpty())
    {
        actionManager()->trigger(AssignCustomGroupIdAction, Parameters(sourceResources)
            .withArgument(Qn::ResourceTreeCustomGroupIdRole, dropGroupId));
        return;
    }

    using namespace entity_resource_tree::resource_grouping;
    const auto dragDimension = compositeIdDimension(dragGroupId);
    if (trimCompositeId(dropGroupId, dragDimension) != dragGroupId)
    {
        actionManager()->trigger(MoveToCustomGroupAction, Parameters(sourceResources)
            .withArgument(Qn::ResourceTreeCustomGroupIdRole, dragGroupId)
            .withArgument(Qn::TargetResourceTreeCustomGroupIdRole, dropGroupId));
    }
}

void ResourceTreeDragDropDecoratorModel::moveResources(
    const QnResourceList& cameras,
    const QnResourceList& webPages,
    const QnResourceList& layouts,
    const QString& dragGroupId,
    const QString& dropGroupId,
    const QnMediaServerResourcePtr& dropParentServer)
{
    const auto moveResourcesToGroup =
        [=](QnResourceList moved)
        {
            moveCustomGroup(moved, dragGroupId, dropGroupId);
        };

    if (ini().webPagesAndIntegrations)
    {
        if (!cameras.empty())
        {
            if (!dropParentServer.isNull()
                && cameras.first()->getParentId() != dropParentServer->getId())
            {
                m_actionHandler->moveResourcesToServer(
                    cameras,
                    dropParentServer,
                    moveResourcesToGroup);
            }
            else
            {
                moveResourcesToGroup(cameras);
            }
        }

        if (!webPages.empty())
        {
            // Web pages can only be moved within parent node such as server node, "Web Pages"
            // group node or "Integrations" group node.
            if (dropParentServer.isNull()
                || (webPages.first()->getParentId() == dropParentServer->getId()))
            {
                moveResourcesToGroup(webPages);
            }
        }
    }
    else
    {
        QnResourceList sourceResources;
        std::copy(cameras.cbegin(), cameras.cend(), std::back_inserter(sourceResources));
        std::copy(webPages.cbegin(), webPages.cend(), std::back_inserter(sourceResources));

        if (sourceResources.empty())
            return;

        if (!dropParentServer.isNull()
            && sourceResources.first()->getParentId() != dropParentServer->getId())
        {
            m_actionHandler->moveResourcesToServer(
                sourceResources,
                dropParentServer,
                moveResourcesToGroup);
        }
        else
        {
            moveResourcesToGroup(sourceResources);
        }
    }
    moveResourcesToGroup(layouts);
}

Qt::DropActions ResourceTreeDragDropDecoratorModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QModelIndex ResourceTreeDragDropDecoratorModel::targetDropIndex(const QModelIndex& dropIndex) const
{
    using NodeType = ResourceTree::NodeType;

    // Dropping on custom resource group node have no other meaning.
    if (hasNodeType(dropIndex, NodeType::customResourceGroup))
        return dropIndex;

    if (hasResourceFlags(dropIndex, Qn::layout))
        return dropIndex;

    // Dropping on resource within custom resource group is the same as dropping on the parent
    // custom resource group node.
    if (hasNodeType(dropIndex.parent(), NodeType::customResourceGroup))
        return dropIndex.parent();

    // Dropping on recorder or multisensor camera resource that is within custom resource group is
    // the same as dropping on the enclosing custom resource group node.
    if (hasNodeType(dropIndex.parent(), NodeType::recorder)
        && hasNodeType(dropIndex.parent().parent(), NodeType::customResourceGroup))
    {
        return dropIndex.parent().parent();
    }

    // Dropping into an layout item is the same as dropping into a layout.
    if (hasNodeType(dropIndex, NodeType::layoutItem))
    {
        if (hasResourceFlags(dropIndex.parent(), Qn::layout))
            return dropIndex.parent();
        return QModelIndex();
    }

    // Dropping into an web page within 'Web Pages' group is the same as dropping into 'Web Pages'
    // node itself.
    if (hasResourceFlags(dropIndex, Qn::web_page)
        && (hasNodeType(dropIndex.parent(), NodeType::webPages)
            || hasNodeType(dropIndex.parent(), NodeType::integrations)))
    {
        return dropIndex.parent();
    }

    // Dropping into accessible layouts is the same as dropping into a user.
    if (hasNodeType(dropIndex, NodeType::sharedLayouts))
    {
        const auto parentIndex = dropIndex.parent();
        if (hasNodeType(parentIndex, NodeType::role) || hasResourceFlags(parentIndex, Qn::user))
            return parentIndex;
        return QModelIndex();
    }

    // Dropping into a camera is the same as dropping into a "Cameras & Devices" top level node
    // (servers are not displayed).
    if (hasNodeType(dropIndex.parent(), NodeType::camerasAndDevices))
        return dropIndex.parent();

    // Dropping into a server camera is the same as dropping into a server (servers are displayed).
    if (hasResourceFlags(dropIndex.parent(), Qn::server))
        return dropIndex.parent();

    // Dropping something into a group of resources:
    // Dropping at a group of cameras -> ResourceTree::NodeType::sharedResources
    // Dropping at a group of layouts -> ResourceTree::NodeType::sharedLayouts
    // In both cases we expect that parent node is user or user role.
    if (hasNodeType(dropIndex, NodeType::sharedResources)
        || hasNodeType(dropIndex, NodeType::sharedLayouts))
    {
        const auto parentIndex = dropIndex.parent();
        if (hasNodeType(parentIndex, NodeType::role) || hasResourceFlags(parentIndex, Qn::user))
            return parentIndex;
        return QModelIndex();
    }

    return dropIndex;
}

QnResourcePool* ResourceTreeDragDropDecoratorModel::resourcePool() const
{
    return m_resourcePool;
}

menu::Manager* ResourceTreeDragDropDecoratorModel::actionManager() const
{
    return m_actionManager;
}

} // namespace nx::vms::client::desktop
