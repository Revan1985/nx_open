// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_entity_builder.h"

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/group_grouping_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/grouping_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/single_item_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_group_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/layout_item_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/showreels_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/videowall_matrices_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/entity/videowall_screens_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/cloud_cross_system_camera_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/health_monitor_resource_item_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/main_tree_resource_item_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item/web_page_decorator.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/item_order/resource_tree_item_order.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/recorder_item_data_helper.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/resource_tree_item_key_source_pool.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_item_factory.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/user_layout_resource_index.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

using namespace nx::vms::client::desktop;

namespace {

using namespace entity_item_model;
using namespace entity_resource_tree;
using namespace nx::vms::api;

using NodeType = ResourceTree::NodeType;
using ResourceItemCreator = std::function<AbstractItemPtr(const QnResourcePtr&)>;
using UuidItemCreator = std::function<AbstractItemPtr(const nx::Uuid&)>;

using UserdDefinedGroupIdGetter = std::function<QString(const QnResourcePtr&, int)>;
using UserdDefinedGroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

using RecorderCameraGroupIdGetter = std::function<QString(const QnResourcePtr&, int)>;
using RecorderGroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

MainTreeResourceItemDecorator::Permissions permissionsSummary(
    const QnUserResourcePtr& user,
    const QnResourcePtr& resource)
{
    if (user.isNull())
    {
        return resource->hasFlags(Qn::local_video) || resource->hasFlags(Qn::local_image)
            ? MainTreeResourceItemDecorator::Permissions{
                .permissions = Qn::ReadPermission, .hasPowerUserPermissions = false}
            : MainTreeResourceItemDecorator::Permissions{
                .permissions = Qn::NoPermissions, .hasPowerUserPermissions = false};
    }

    const auto accessManager = user->systemContext()->resourceAccessManager();

    return {
        .permissions = accessManager->permissions(user, resource),
        .hasPowerUserPermissions = accessManager->hasPowerUserPermissions(user)
    };
}

ResourceItemCreator serverResourceItemCreator(
    ResourceTreeItemFactory* factory,
    const QnUserResourcePtr& user)
{
    return
        [factory, user](const QnResourcePtr& resource)
        {
            const auto isEdgeCamera =
                !resource->hasFlags(Qn::server) && resource.dynamicCast<QnVirtualCameraResource>();

            const auto nodeType = isEdgeCamera ? NodeType::edge : NodeType::resource;

            return std::make_unique<MainTreeResourceItemDecorator>(
                factory->createResourceItem(resource),
                permissionsSummary(user, resource),
                nodeType);
        };
}

ResourceItemCreator simpleResourceItemCreator(ResourceTreeItemFactory* factory)
{
    return
        [factory](const QnResourcePtr& resource) { return factory->createResourceItem(resource); };
}

AbstractItemPtr createDecoratedResourceItem(
    ResourceTreeItemFactory* factory,
    const QnResourcePtr& resource,
    bool hasPowerUserPermissions)
{
    return resource->hasFlags(Qn::web_page)
        ? std::make_unique<WebPageDecorator>(
            factory->createResourceItem(resource), hasPowerUserPermissions)
        : factory->createResourceItem(resource);
}

ResourceItemCreator resourceItemCreator(
    ResourceTreeItemFactory* factory,
    const QnUserResourcePtr& user,
    bool hasPowerUserPermissions = false)
{
    return
        [=](const QnResourcePtr& resource)
        {
            return std::make_unique<MainTreeResourceItemDecorator>(
                createDecoratedResourceItem(factory, resource, hasPowerUserPermissions),
                permissionsSummary(user, resource),
                NodeType::resource);
        };
}

LayoutItemCreator layoutItemCreator(
    ResourceTreeItemFactory* factory,
    const QnUserResourcePtr& user,
    const QnLayoutResourcePtr& layout,
    bool hasPowerUserPermissions)
{
    return
        [=](const nx::Uuid& itemId) -> AbstractItemPtr
        {
            const auto itemData = layout->getItem(itemId);

            const auto itemResource =
                nx::vms::client::core::getResourceByDescriptor(itemData.resource);
            const auto permSummary = permissionsSummary(user, itemResource);
            if (permSummary.permissions == Qn::NoPermissions)
                return {};

            return std::make_unique<MainTreeResourceItemDecorator>(
                createDecoratedResourceItem(factory, itemResource, hasPowerUserPermissions),
                permSummary,
                NodeType::layoutItem,
                itemData.uuid);
        };
}

ResourceItemCreator webPageItemCreator(
    ResourceTreeItemFactory* factory,
    bool hasPowerUserPermissions)
{
    return
        [=](const QnResourcePtr& resource) -> AbstractItemPtr
        {
            return std::make_unique<WebPageDecorator>(
                factory->createResourceItem(resource),
                hasPowerUserPermissions);
        };
}

UuidItemCreator otherServerItemCreator(
    ResourceTreeItemFactory* factory,
    const QnUserResourcePtr& /*user*/)
{
    return
        [=](const nx::Uuid& serverId) -> AbstractItemPtr
        {
            return factory->createOtherServerItem(serverId);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to Recorders and Multisensor cameras.
//-------------------------------------------------------------------------------------------------

RecorderCameraGroupIdGetter recorderCameraGroupIdGetter()
{
    return
        [](const QnResourcePtr& resource, int /*order*/)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                return camera->getGroupId();
            return QString();
        };
}

RecorderGroupItemCreator recorderGroupItemCreator(
    ResourceTreeItemFactory* factory,
    const QSharedPointer<RecorderItemDataHelper>& recorderResourceIndex,
    const QnUserResourcePtr& user)
{
    return
        [factory, recorderResourceIndex, user](const QString& groupId) -> AbstractItemPtr
        {
            Qt::ItemFlags flags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled};

            flags.setFlag(Qt::ItemIsEditable,
                recorderResourceIndex->hasPermissions(
                    groupId,
                    user,
                    Qn::WriteNamePermission | Qn::SavePermission));

            return factory->createRecorderItem(groupId, recorderResourceIndex, flags);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to the custom grouping within Resource Tree
//-------------------------------------------------------------------------------------------------

UserdDefinedGroupIdGetter userDefinedGroupIdGetter()
{
    return
        [](const QnResourcePtr& resource, int order)
        {
            const auto compositeGroupId = resource_grouping::resourceCustomGroupId(resource);

            if (resource_grouping::compositeIdDimension(compositeGroupId) <= order)
                return QString();

            return resource_grouping::trimCompositeId(compositeGroupId, order + 1);
        };
}

UserdDefinedGroupItemCreator userDefinedGroupItemCreator(
    ResourceTreeItemFactory* factory,
    bool hasPowerUserPermissions)
{
    return
        [factory, hasPowerUserPermissions](const QString& compositeGroupId) -> AbstractItemPtr
        {
            Qt::ItemFlags result =
                {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled};

            if (hasPowerUserPermissions)
                result |= {Qt::ItemIsEditable, Qt::ItemIsDropEnabled};

            return factory->createResourceGroupItem(compositeGroupId, result);
        };
}

//-------------------------------------------------------------------------------------------------
// Function wrapper providers related to the grouping by server in resource selection dialogs.
//-------------------------------------------------------------------------------------------------
std::function<QString(const QnResourcePtr&, int)> parentServerIdGetter()
{
    return
        [](const QnResourcePtr& resource, int /*order*/)
        {
            return resource->getParentId().toString(QUuid::WithBraces);
        };
}

std::function<AbstractItemPtr(const QString&)> parentServerItemCreator(
    const QnResourcePool* resourcePool, ResourceTreeItemFactory* factory)
{
    return
        [resourcePool, factory](const QString& serverId) -> AbstractItemPtr
        {
            const auto serverResource =
                resourcePool->getResourceById(nx::Uuid::fromStringSafe(serverId));
            return factory->createResourceItem(serverResource);
        };
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace entity_item_model;
using namespace nx::vms::api;

ResourceTreeEntityBuilder::ResourceTreeEntityBuilder(SystemContext* systemContext):
    base_type(),
    SystemContextAware(systemContext),
    m_cameraResourceIndex(new CameraResourceIndex(resourcePool())),
    m_userLayoutResourceIndex(new UserLayoutResourceIndex(resourcePool())),
    m_recorderItemDataHelper(new RecorderItemDataHelper(m_cameraResourceIndex.get())),
    m_itemFactory(new ResourceTreeItemFactory(systemContext)),
    m_itemKeySourcePool(new ResourceTreeItemKeySourcePool(
        systemContext, m_cameraResourceIndex.get(), m_userLayoutResourceIndex.get()))
{
    // TODO: #sivanov There should be more elegant way to handle unit tests limitations.
    // Message processor does not exist in unit tests.
    if (messageProcessor())
    {
        auto userWatcher = new core::SessionResourcesSignalListener<QnUserResource>(
            systemContext,
            this);
        userWatcher->setOnRemovedHandler(
            [this](const QnUserResourceList& users)
            {
                if (m_user && users.contains(m_user))
                    m_user.reset();
            });
        userWatcher->start();
    }
}

ResourceTreeEntityBuilder::~ResourceTreeEntityBuilder()
{
}

QnUserResourcePtr ResourceTreeEntityBuilder::user() const
{
    return m_user;
}

void ResourceTreeEntityBuilder::setUser(const QnUserResourcePtr& user)
{
    m_user = user;
}

bool ResourceTreeEntityBuilder::hasPowerUserPermissions() const
{
    return !user().isNull()
        ? systemContext()->resourceAccessManager()->hasPowerUserPermissions(user())
        : false;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createServersGroupEntity(
    bool showProxiedResources) const
{
    auto serverExpander =
        [this, showProxiedResources](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::server))
                return AbstractEntityPtr();

            return createServerCamerasEntity(
                resource.staticCast<QnMediaServerResource>(), showProxiedResources);
        };

    auto serversGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
        serverResourceItemCreator(m_itemFactory.get(), user()),
        serverExpander,
        serversOrder());

    serversGroupList->installItemSource(
        m_itemKeySourcePool->serversSource(user(), /*reduceEdgeServers*/ true));

    return makeFlatteningGroup(
        m_itemFactory->createServersItem(),
        std::move(serversGroupList),
        FlatteningGroupEntity::AutoFlatteningPolicy::singleChildPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createHealthMonitorsGroupEntity() const
{
    const auto healthMonitorItemCreator =
        [this](const QnResourcePtr& resource)
        {
            return std::make_unique<HealthMonitorResourceItemDecorator>(
                serverResourceItemCreator(m_itemFactory.get(), user())(resource));
        };

    auto healthMonitorsList = makeKeyList<QnResourcePtr>(
        healthMonitorItemCreator,
        serversOrder());

    healthMonitorsList->installItemSource(
        m_itemKeySourcePool->healthMonitorsSource(user()));

    return makeFlatteningGroup(
        m_itemFactory->createHealthMonitorsItem(),
        std::move(healthMonitorsList),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCamerasAndDevicesGroupEntity(
    bool showProxiedResources) const
{
    Qt::ItemFlags camerasAndDevicesitemFlags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
    if (hasPowerUserPermissions())
        camerasAndDevicesitemFlags |= Qt::ItemIsDropEnabled;

    const auto showServersInTreeForNonAdmins =
        systemContext()->globalSettings()->showServersInTreeForNonAdmins();

    const FlatteningGroupEntity::AutoFlatteningPolicy flatteningPolicy =
        showServersInTreeForNonAdmins || hasPowerUserPermissions()
            ? FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy
            : FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy;

    return makeFlatteningGroup(
        m_itemFactory->createCamerasAndDevicesItem(camerasAndDevicesitemFlags),
        createServerCamerasEntity(QnMediaServerResourcePtr(), showProxiedResources),
        flatteningPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllCamerasEntity(
    bool showServers,
    const std::function<bool(const QnResourcePtr&)>& resourceFilter,
    bool permissionsHandledByFilter) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        {});

    GroupingRuleStack groupingRuleStack;

    if (showServers)
    {
        groupingRuleStack.push_back(
            {parentServerIdGetter(),
            parentServerItemCreator(resourcePool(), m_itemFactory.get()),
            Qn::ParentResourceRole,
            1,
            numericOrder()});
    }

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), /*hasPowerUserPermissions*/ false),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(permissionsHandledByFilter
        ? m_itemKeySourcePool->allDevicesSource({}, resourceFilter)
        : m_itemKeySourcePool->allDevicesSource(user(), resourceFilter));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllCamerasAndResourcesEntity(
    bool withProxiedWebPages) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        {});

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), /*hasPowerUserPermissions*/ false),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    if (withProxiedWebPages)
    {
        camerasGroupingEntity->installItemSource(
            m_itemKeySourcePool->devicesAndProxiedWebPagesSource(
                QnMediaServerResourcePtr(),
                user()));
    }
    else
    {
        camerasGroupingEntity->installItemSource(
            m_itemKeySourcePool->devicesSource(user(),
                QnMediaServerResourcePtr(),
                /*resourceFilter*/ nullptr));
    }

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogServerCamerasEntity(
    const QnMediaServerResourcePtr& server,
    const std::function<bool(const QnResourcePtr&)>& resourceFilter) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        {});

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), /*hasPowerUserPermissions*/ false),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        simpleResourceItemCreator(m_itemFactory.get()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->devicesSource(user(), server, resourceFilter));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogAllLayoutsEntity() const
{
    auto allLayoutsList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()), layoutsOrder());

    allLayoutsList->installItemSource(m_itemKeySourcePool->allLayoutsSource(user()));

    return allLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogShareableLayoutsEntity(
    const std::function<bool(const QnResourcePtr&)>& resourceFilter) const
{
    auto sharedLayoutsList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()), layoutsOrder());

    sharedLayoutsList->installItemSource(
        m_itemKeySourcePool->shareableLayoutsSource(user(), resourceFilter));

    return sharedLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogHotspotTargetsEntity(
    bool showServers,
    const std::function<bool(const QnResourcePtr&)>& resourceFilter) const
{
    static const Qt::ItemFlags kRootItemsFlags = {Qt::ItemIsEnabled | Qt::ItemIsSelectable};
    auto composition = std::make_unique<CompositionEntity>();
    if (showServers)
    {
        composition->addSubEntity(makeFlatteningGroup(
            m_itemFactory->createServersItem(),
            createDialogAllCamerasEntity(showServers, resourceFilter),
            FlatteningGroupEntity::AutoFlatteningPolicy::singleChildPolicy));
    }
    else
    {
        composition->addSubEntity(makeFlatteningGroup(
            m_itemFactory->createCamerasAndDevicesItem(kRootItemsFlags),
            createDialogAllCamerasEntity(showServers, resourceFilter),
            FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy));
    }
    composition->addSubEntity(makeFlatteningGroup(
        m_itemFactory->createLayoutsItem(kRootItemsFlags),
        createDialogShareableLayoutsEntity(resourceFilter),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy));

    return composition;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllServersEntity() const
{
    auto allServersList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), user()), numericOrder());

    allServersList->installItemSource(
        m_itemKeySourcePool->serversSource(user(), /*reduceEdgeServers*/ true));

    return allServersList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllCamerasEntity() const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        user());

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(m_itemFactory.get(), user()),
        core::ResourceRole,
        serverResourcesOrder(),
        GroupingRuleStack{recordersGroupingRule});

    camerasGroupingEntity->installItemSource(
        m_itemKeySourcePool->allDevicesSource(user(), /*resourceFilter*/ {}));

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createAllLayoutsEntity() const
{
    auto allLayoutsList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), user()),
        layoutsOrder());

    allLayoutsList->installItemSource(m_itemKeySourcePool->allLayoutsSource(user()));

    return allLayoutsList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createFlatCamerasListEntity() const
{
    auto allCamerasList = makeKeyList<QnResourcePtr>(
        simpleResourceItemCreator(m_itemFactory.get()), numericOrder());

    allCamerasList->installItemSource(m_itemKeySourcePool->devicesSource(user(), {}, {}));

    return allCamerasList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createServerCamerasEntity(
    const QnMediaServerResourcePtr& server,
    bool showProxiedResources) const
{
    using GroupingRule = GroupingRule<QString, QnResourcePtr>;
    using GroupingRuleStack = GroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    const auto recorderGroupCreator = recorderGroupItemCreator(
        m_itemFactory.get(),
        m_recorderItemDataHelper,
        user());

    GroupingRuleStack groupingRuleStack;

    groupingRuleStack.push_back(
        {userDefinedGroupIdGetter(),
        userDefinedGroupItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
        Qn::ResourceTreeCustomGroupIdRole,
        resource_grouping::kUserDefinedGroupingDepth,
        numericOrder()});

    const GroupingRule recordersGroupingRule =
        {recorderCameraGroupIdGetter(),
        recorderGroupCreator,
        Qn::CameraGroupIdRole,
        1, //< Dimension.
        numericOrder()};
    groupingRuleStack.push_back(recordersGroupingRule);

    auto camerasGroupingEntity = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
        resourceItemCreator(m_itemFactory.get(), user(), hasPowerUserPermissions()),
        core::ResourceRole,
        serverResourcesOrder(),
        groupingRuleStack);

    if (ini().webPagesAndIntegrations && !showProxiedResources)
    {
        camerasGroupingEntity->installItemSource(
            m_itemKeySourcePool->devicesSource(user(), server, /*resourceFilter*/ {}));
    }
    else
    {
        camerasGroupingEntity->installItemSource(
            m_itemKeySourcePool->devicesAndProxiedWebPagesSource(server, user()));
    }

    return camerasGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCurrentSystemEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createCurrentSystemItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCurrentUserEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createCurrentUserItem(user()));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSpacerEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createSpacerItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createSeparatorEntity() const
{
    return makeSingleItemEntity(m_itemFactory->createSeparatorItem());
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLayoutItemListEntity(
    const QnResourcePtr& layoutResource) const
{
    if (!layoutResource || layoutResource->hasFlags(Qn::removed))
        return {};

    if (!layoutResource->hasFlags(Qn::layout))
        return {};

    const auto layout = layoutResource.staticCast<QnLayoutResource>();

    return std::make_unique<LayoutItemListEntity>(layout,
        layoutItemCreator(m_itemFactory.get(), user(), layout, hasPowerUserPermissions()));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLayoutsGroupEntity() const
{
    using GroupingRuleStack = GroupGroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

    auto layoutItemCreator =
        [this, baseItemCreator = resourceItemCreator(m_itemFactory.get(), user())]
            (const QnResourcePtr& resource)
        {
            auto layout = resource.objectCast<QnLayoutResource>();
            NX_ASSERT(layout);

            if (layout->hasFlags(Qn::cross_system))
                return m_itemFactory->createCloudLayoutItem(layout);
            else
                return baseItemCreator(layout);
        };

    AbstractEntityPtr layoutsEntity;
    if (ini().foldersForLayoutsInResourceTree)
    {
        GroupingRuleStack groupingRuleStack;

        groupingRuleStack.push_back({userDefinedGroupIdGetter(),
            userDefinedGroupItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
            Qn::ResourceTreeCustomGroupIdRole,
            resource_grouping::kUserDefinedGroupingDepth,
            numericOrder()});

        auto layoutsGroupGroupingList = std::make_unique<GroupGroupingEntity<QString, QnResourcePtr>>(
            layoutItemCreator,
            [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
            core::ResourceRole,
            layoutsOrder(),
            groupingRuleStack);

        layoutsGroupGroupingList->installItemSource(m_itemKeySourcePool->layoutsSource(user()));
        layoutsEntity = std::move(layoutsGroupGroupingList);
    }
    else
    {
        auto layoutsGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
            layoutItemCreator,
            [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
            layoutsOrder());
        layoutsGroupList->installItemSource(m_itemKeySourcePool->layoutsSource(user()));
        layoutsEntity = std::move(layoutsGroupList);
    }

    Qt::ItemFlags rootItemFlags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
    if (hasPowerUserPermissions())
        rootItemFlags |= Qt::ItemIsDropEnabled;

    return makeFlatteningGroup(
        m_itemFactory->createLayoutsItem(rootItemFlags),
        std::move(layoutsEntity),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createShowreelsGroupEntity() const
{
    const auto showreelItemCreator =
        [this](const nx::Uuid& id) { return m_itemFactory->createShowreelItem(id); };

    const auto showreelManager = systemContext()->showreelManager();

    return makeFlatteningGroup(
        m_itemFactory->createShowreelsItem(),
        std::make_unique<ShowreelsListEntity>(showreelItemCreator, showreelManager),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createIntegrationsGroupEntity() const
{
    auto integrationsListEntity = makeKeyList<QnResourcePtr>(
        webPageItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
        numericOrder());

    integrationsListEntity->installItemSource(m_itemKeySourcePool->integrationsSource(
        user(), /*includeProxiedIntegrations*/ true));

    Qt::ItemFlags flags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
    if (hasPowerUserPermissions())
        flags |= Qt::ItemIsDropEnabled;

    return makeFlatteningGroup(
        m_itemFactory->createIntegrationsItem(flags),
        std::move(integrationsListEntity),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createWebPagesGroupEntity() const
{
    ResourceItemCreator itemCreator = ini().webPagesAndIntegrations
        ? webPageItemCreator(m_itemFactory.get(), hasPowerUserPermissions())
        : resourceItemCreator(m_itemFactory.get(), user(), hasPowerUserPermissions());

    auto webPagesListEntity = makeKeyList<QnResourcePtr>(itemCreator, numericOrder());

    if (ini().webPagesAndIntegrations)
    {
        webPagesListEntity->installItemSource(
            m_itemKeySourcePool->webPagesSource(user(), /*includeProxiedWebPages*/ true));
    }
    else
    {
        webPagesListEntity->installItemSource(
            m_itemKeySourcePool->webPagesAndIntegrationsSource(user(), /*includeProxied*/ false));
    }

    Qt::ItemFlags flags = {Qt::ItemIsEnabled, Qt::ItemIsSelectable};
    if (hasPowerUserPermissions())
        flags |= Qt::ItemIsDropEnabled;

    const auto flatteningPolicy = hasPowerUserPermissions()
        ? FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy
        : FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy;

    return makeFlatteningGroup(
        m_itemFactory->createWebPagesItem(flags),
        std::move(webPagesListEntity),
        flatteningPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLocalFilesGroupEntity() const
{
    auto fileLayoutsGroupList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), user()),
        [this](const QnResourcePtr& resource) { return createLayoutItemListEntity(resource); },
        numericOrder());

    fileLayoutsGroupList->installItemSource(m_itemKeySourcePool->fileLayoutsSource());

    auto mediaFilesList = makeKeyList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), user()),
        locaResourcesOrder());

    mediaFilesList->installItemSource(m_itemKeySourcePool->localMediaSource());

    auto localResourceComposition = std::make_unique<CompositionEntity>();
    localResourceComposition->addSubEntity(std::move(fileLayoutsGroupList));
    localResourceComposition->addSubEntity(std::move(mediaFilesList));

    return makeFlatteningGroup(m_itemFactory->createLocalFilesItem(),
        std::move(localResourceComposition));
}

AbstractEntityPtr ResourceTreeEntityBuilder::createVideowallsEntity() const
{
    auto videowallExpander =
        [this](const QnResourcePtr& resource)
        {
            const auto videoWall = resource.dynamicCast<QnVideoWallResource>();

            const auto videoWallScreenCreator =
                [this, videoWall](const nx::Uuid& id)
                {
                    return m_itemFactory->createVideoWallScreenItem(videoWall, id);
                };

            const auto videoWallMatrixCreator =
                [this, videoWall](const nx::Uuid& id)
                {
                    return m_itemFactory->createVideoWallMatrixItem(videoWall, id);
                };

            auto childComposition = std::make_unique<CompositionEntity>();

            childComposition->addSubEntity(
                std::make_unique<VideoWallScreensEntity>(videoWallScreenCreator, videoWall));
            childComposition->addSubEntity(
                std::make_unique<VideowallMatricesEntity>(videoWallMatrixCreator, videoWall));

            return childComposition;
        };

    auto videowallList = makeUniqueKeyGroupList<QnResourcePtr>(
        resourceItemCreator(m_itemFactory.get(), user()),
        videowallExpander,
        numericOrder());

    videowallList->installItemSource(m_itemKeySourcePool->videoWallsSource(user()));

    return videowallList;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createLocalOtherSystemsEntity() const
{
    using GroupingRule = GroupingRule<QString, nx::Uuid>;
    using GroupingRuleStack = GroupingEntity<QString, nx::Uuid>::GroupingRuleStack;

    const auto systemNameGetter =
        [this](const nx::Uuid& serverId, int /*order*/) -> QString
        {
            const auto moduleInformation =
                systemContext()->otherServersManager()->getModuleInformationWithAddresses(serverId);

            return moduleInformation.isNewSystem()
                ? tr("New Site")
                : moduleInformation.systemName;
        };

    const auto systemItemCreator =
        [this](const QString& systemName) -> AbstractItemPtr
        {
            return m_itemFactory->createOtherSystemItem(systemName);
        };

    const GroupingRule otherSystemsGroupingRule =
        {systemNameGetter,
        systemItemCreator,
        Qt::DisplayRole,
        1, //< Dimension
        numericOrder()};

    auto otherSystemsGroupingEntity = std::make_unique<GroupingEntity<QString, nx::Uuid>>(
        otherServerItemCreator(m_itemFactory.get(), user()),
        core::UuidRole,
        numericOrder(),
        GroupingRuleStack{otherSystemsGroupingRule});

    otherSystemsGroupingEntity->installItemSource(m_itemKeySourcePool->otherServersSource());

    return otherSystemsGroupingEntity;
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCloudOtherSystemsEntity() const
{
    auto systemItemCreator =
        [this](const QString& systemId) -> AbstractItemPtr
        {
            return m_itemFactory->createCloudSystemItem(systemId);
        };

    auto cameraListCreator =
        [this](const QString& systemId) -> AbstractEntityPtr
        {
            return createCloudSystemCamerasEntity(systemId);
        };

    if (user() && user()->isCloud())
    {
        auto cloudSystemsEntity =
            makeUniqueKeyGroupList<QString>(systemItemCreator, cameraListCreator, numericOrder());
        cloudSystemsEntity->installItemSource(m_itemKeySourcePool->cloudSystemsSource());
        return cloudSystemsEntity;
    }
    else
    {
        auto cloudSystemsEntity = makeKeyList<QString>(systemItemCreator, numericOrder());
        cloudSystemsEntity->installItemSource(m_itemKeySourcePool->cloudSystemsSource());
        return cloudSystemsEntity;
    }
}

AbstractEntityPtr ResourceTreeEntityBuilder::createCloudSystemCamerasEntity(
    const QString& systemId) const
{
    auto itemCreator =
        [this](const QnResourcePtr& camera)
        {
            return std::make_unique<CloudCrossSystemCameraDecorator>(
                m_itemFactory->createResourceItem(camera));
        };

    auto list = makeKeyList<QnResourcePtr>(itemCreator, numericOrder());
    list->installItemSource(m_itemKeySourcePool->cloudSystemCamerasSource(systemId));

    return makeFlatteningGroup(
        m_itemFactory->createCloudSystemStatusItem(systemId),
        std::move(list),
        FlatteningGroupEntity::AutoFlatteningPolicy::headItemControl);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createOtherSystemsGroupEntity() const
{
    auto otherSystemsComposition = std::make_unique<CompositionEntity>();
    otherSystemsComposition->addSubEntity(createCloudOtherSystemsEntity());
    if (user() && user()->isAdministrator())
        otherSystemsComposition->addSubEntity(createLocalOtherSystemsEntity());

    return makeFlatteningGroup(
        m_itemFactory->createOtherSystemsItem(),
        std::move(otherSystemsComposition),
        FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy);
}

AbstractEntityPtr ResourceTreeEntityBuilder::createDialogEntities(
    ResourceTree::ResourceFilters resourceTypes,
    bool alwaysCreateGroupElements,
    bool combineWebPagesAndIntegrations) const
{
    static const Qt::ItemFlags kRootItemsFlags = {Qt::ItemIsEnabled | Qt::ItemIsSelectable};

    std::vector<AbstractEntityPtr> entities;

    const bool maySkipGroup = !alwaysCreateGroupElements;

    const auto flatteningPolicy = alwaysCreateGroupElements
        ? FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy
        : FlatteningGroupEntity::AutoFlatteningPolicy::noChildrenPolicy;

    if (resourceTypes.testFlag(ResourceTree::ResourceFilter::camerasAndDevices))
    {
        auto entity = createDialogAllCamerasAndResourcesEntity(/*withProxiedWebPages*/ false);
        if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::camerasAndDevices)
            return entity; //< Only cameras and devices, return without a group element.

        entities.push_back(makeFlatteningGroup(
            m_itemFactory->createCamerasAndDevicesItem(kRootItemsFlags),
            std::move(entity),
            flatteningPolicy));
    }

    if (resourceTypes.testFlag(ResourceTree::ResourceFilter::layouts))
    {
        AbstractEntityPtr layoutsEntity;
        if (ini().foldersForLayoutsInResourceTree)
        {
            using GroupingRule = GroupingRule<QString, QnResourcePtr>;
            using GroupingRuleStack =
                GroupGroupingEntity<QString, QnResourcePtr>::GroupingRuleStack;

            GroupingRuleStack groupingRuleStack;
            groupingRuleStack.push_back({userDefinedGroupIdGetter(),
                userDefinedGroupItemCreator(m_itemFactory.get(), /*hasPowerUserPermissions*/ false),
                Qn::ResourceTreeCustomGroupIdRole,
                resource_grouping::kUserDefinedGroupingDepth,
                numericOrder()});

            auto layoutsGroupList = std::make_unique<GroupingEntity<QString, QnResourcePtr>>(
                simpleResourceItemCreator(m_itemFactory.get()),
                core::ResourceRole,
                layoutsOrder(),
                groupingRuleStack);

            layoutsGroupList->installItemSource(
                m_itemKeySourcePool->shareableLayoutsSource(user()));
            layoutsEntity = std::move(layoutsGroupList);
        }
        else
        {
            auto layoutsList = makeKeyList<QnResourcePtr>(
                simpleResourceItemCreator(m_itemFactory.get()), layoutsOrder());

            layoutsList->installItemSource(m_itemKeySourcePool->shareableLayoutsSource(user()));
            layoutsEntity = std::move(layoutsList);
        }

        if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::layouts)
            return layoutsEntity; //< Only layouts, return without a group element.

        entities.push_back(makeFlatteningGroup(
            m_itemFactory->createLayoutsItem(kRootItemsFlags),
            std::move(layoutsEntity),
            flatteningPolicy));
    }

    constexpr ResourceTree::ResourceFilters webPagesAndIntegrations =
        ResourceTree::ResourceFilter::webPages
        | ResourceTree::ResourceFilter::integrations;

    if (combineWebPagesAndIntegrations && resourceTypes.testFlags(webPagesAndIntegrations))
    {
        auto webPagesList = makeKeyList<QnResourcePtr>(
            webPageItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
            numericOrder());

        webPagesList->installItemSource(
            m_itemKeySourcePool->webPagesAndIntegrationsSource(
                user(), /*includeProxied*/ true));

        // If only web pages & integrations, return without a group element.
        if (maySkipGroup && resourceTypes == (int) webPagesAndIntegrations)
            return webPagesList;

        if (ini().webPagesAndIntegrations)
        {
            entities.push_back(makeFlatteningGroup(
                m_itemFactory->createWebPagesAndIntegrationsItem(
                    Qt::ItemIsEnabled | Qt::ItemIsSelectable),
                std::move(webPagesList),
                flatteningPolicy));
        }
        else
        {
            entities.push_back(makeFlatteningGroup(
                m_itemFactory->createWebPagesItem(Qt::ItemIsEnabled | Qt::ItemIsSelectable),
                std::move(webPagesList),
                flatteningPolicy));
        }
    }
    else
    {
        if (resourceTypes.testFlag(ResourceTree::ResourceFilter::integrations)
            && ini().webPagesAndIntegrations)
        {
            auto integrationList = makeKeyList<QnResourcePtr>(
                webPageItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
                numericOrder());

            integrationList->installItemSource(
                m_itemKeySourcePool->integrationsSource(user(), /*includeProxied*/ true));

            if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::integrations)
                return integrationList; //< Only integrations, return without a group element.

            entities.push_back(makeFlatteningGroup(
                m_itemFactory->createIntegrationsItem(Qt::ItemIsEnabled | Qt::ItemIsSelectable),
                std::move(integrationList),
                flatteningPolicy));
        }

        if (resourceTypes.testFlag(ResourceTree::ResourceFilter::webPages))
        {
            auto webPagesList = makeKeyList<QnResourcePtr>(
                webPageItemCreator(m_itemFactory.get(), hasPowerUserPermissions()),
                numericOrder());

            if (ini().webPagesAndIntegrations)
            {
                webPagesList->installItemSource(
                    m_itemKeySourcePool->webPagesSource(user(), /*includeProxiedWebPages*/ true));
            }
            else
            {
                webPagesList->installItemSource(
                    m_itemKeySourcePool->webPagesAndIntegrationsSource(
                        user(), /*includeProxied*/ true));
            }

            if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::webPages)
                return webPagesList; //< Only web pages, return without a group element.

            entities.push_back(makeFlatteningGroup(
                m_itemFactory->createWebPagesItem(Qt::ItemIsEnabled | Qt::ItemIsSelectable),
                std::move(webPagesList),
                flatteningPolicy));
        }
    }

    if (resourceTypes.testFlag(ResourceTree::ResourceFilter::healthMonitors))
    {
        const auto healthMonitorItemCreator =
            [this](const QnResourcePtr& resource)
            {
                return std::make_unique<HealthMonitorResourceItemDecorator>(
                    m_itemFactory->createResourceItem(resource));
            };

        auto healthMonitorsList = makeKeyList<QnResourcePtr>(
            healthMonitorItemCreator,
            serversOrder());

        healthMonitorsList->installItemSource(
            m_itemKeySourcePool->serversSource(user(), /*reduceEdgeServers*/ false));

        if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::healthMonitors)
            return healthMonitorsList; //< Only health monitors, return without a group element.

        entities.push_back(makeFlatteningGroup(
            m_itemFactory->createHealthMonitorsItem(),
            std::move(healthMonitorsList),
            flatteningPolicy));
    }

    if (resourceTypes.testFlag(ResourceTree::ResourceFilter::videoWalls))
    {
        auto videoWallList = makeKeyList<QnResourcePtr>(
            simpleResourceItemCreator(m_itemFactory.get()),
            numericOrder());

        videoWallList->installItemSource(
            m_itemKeySourcePool->videoWallsSource(user()));

        if (maySkipGroup && resourceTypes == (int) ResourceTree::ResourceFilter::videoWalls)
            return videoWallList; //< Only videowalls, return without a group element.

        entities.push_back(makeFlatteningGroup(
            m_itemFactory->createVideoWallsItem(),
            std::move(videoWallList),
            flatteningPolicy));
    }

    auto composition =
        std::make_unique<entity_item_model::CompositionEntity>();

    for (int i = 0; i < (int)entities.size(); ++i)
        composition->addSubEntity(std::move(entities[i]));

    return composition;
}

AbstractEntityPtr ResourceTreeEntityBuilder::addPinnedItem(
    AbstractEntityPtr baseEntity,
    AbstractItemPtr pinnedItem,
    bool withSeparator) const
{
    auto result = std::make_unique<CompositionEntity>();
    if (withSeparator)
        result->addSubEntity(createSpacerEntity());
    result->addSubEntity(makeSingleItemEntity(std::move(pinnedItem)));
    if (withSeparator)
        result->addSubEntity(createSeparatorEntity());
    result->addSubEntity(std::move(baseEntity));
    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
