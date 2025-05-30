// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_test_helper.h"

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/storage_resource_stub.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::api;

QnUserResourcePtr QnResourcePoolTestHelper::createUser(
    Ids parentGroupIds,
    const QString& name,
    UserType userType,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    QnUserResourcePtr user(new QnUserResource(userType, {ldapDn}));
    user->setIdUnsafe(nx::Uuid::createUuid());
    user->setName(name);
    user->setPasswordAndGenerateHash(name);
    user->setSiteGroupIds(parentGroupIds.data);
    user->setRawPermissions(globalPermissions);
    user->setResourceAccessRights(resourceAccessRights);
    user->addFlags(Qn::remote);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addUser(
    Ids parentGroupIds,
    const QString& name,
    UserType userType,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    const QString& ldapDn)
{
    const auto user = createUser(
        parentGroupIds, name, userType, globalPermissions, resourceAccessRights, ldapDn);
    resourcePool()->addResource(user);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addOrgUser(
    Ids orgGroupIds,
    const QString& name,
    GlobalPermissions globalPermissions,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights)
{
    auto user = createUser(
        NoGroup, name, nx::vms::api::UserType::cloud, globalPermissions, resourceAccessRights);
    user->setOrgGroupIds(orgGroupIds.data);
    resourcePool()->addResource(user);
    return user;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::createLayout()
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->setName(nx::Uuid::createUuid().toSimpleString());
    return layout;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayout()
{
    auto layout = createLayout();
    resourcePool()->addResource(layout);
    return layout;
}

nx::Uuid QnResourcePoolTestHelper::addToLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourcePtr& resource)
{
    nx::vms::common::LayoutItemData item;
    item.uuid = nx::Uuid::createUuid();
    item.resource.id = resource->getId();
    layout->addItem(item);
    return item.uuid;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::createCamera(Qn::LicenseType licenseType)
{
    nx::CameraResourceStubPtr camera(new nx::CameraResourceStub(licenseType));
    camera->setName("camera");
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addCamera(Qn::LicenseType licenseType)
{
    auto camera = createCamera(licenseType);
    resourcePool()->addResource(camera);
    return camera;
}

std::vector<nx::CameraResourceStubPtr> QnResourcePoolTestHelper::addCameras(size_t count)
{
    std::vector<nx::CameraResourceStubPtr> cameras;
    cameras.reserve(count);
    for (size_t i = 0; i < count; ++i)
        cameras.push_back(addCamera());
    return cameras;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::createDesktopCamera(
    const QnUserResourcePtr& user)
{
    auto camera = createCamera(Qn::LicenseType::LC_Invalid);
    camera->setTypeId(CameraData::kDesktopCameraTypeId);
    camera->setFlags(Qn::desktop_camera);
    camera->setModel("virtual desktop camera"); //< TODO: #sivanov Globalize the constant.
    camera->setName(user->getName());
    camera->setPhysicalId(
        nx::Uuid::createUuid().toString(QUuid::WithBraces) //< Running client instance id must be here.
        + user->getId().toString(QUuid::WithBraces));
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addDesktopCamera(
    const QnUserResourcePtr& user)
{
    auto camera = createDesktopCamera(user);
    resourcePool()->addResource(camera);
    return camera;
}

void QnResourcePoolTestHelper::toIntercom(nx::CameraResourceStubPtr camera)
{
    QnIOPortData intercomFeaturePort;
    intercomFeaturePort.outputName = QnVirtualCameraResource::intercomSpecificPortName();

    camera->setProperty(
        nx::vms::api::device_properties::kIoSettings,
        QString::fromStdString(nx::reflect::json::serialize(
            QnIOPortDataList{intercomFeaturePort})));
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addIntercomCamera()
{
    const auto camera = createCamera();
    toIntercom(camera);
    resourcePool()->addResource(camera);
    return camera;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addIntercomLayout(
    const QnVirtualCameraResourcePtr& intercomCamera)
{
    if (!NX_ASSERT(intercomCamera))
        return {};

    const auto intercomId = intercomCamera->getId();

    const auto layout = createLayout();
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(intercomId));
    layout->setParentId(intercomId);

    resourcePool()->addResource(layout);
    NX_ASSERT(!intercomCamera->isIntercom()
        || intercomCamera->isIntercom() && nx::vms::common::isIntercomLayout(layout));
    return layout;
}

QnWebPageResourcePtr QnResourcePoolTestHelper::addWebPage()
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setIdUnsafe(nx::Uuid::createUuid());
    webPage->setUrl("http://www.ru");
    webPage->setName(QnWebPageResource::nameForUrl(webPage->getUrl()));
    resourcePool()->addResource(webPage);
    return webPage;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::createVideoWall()
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setIdUnsafe(nx::Uuid::createUuid());
    return videoWall;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::addVideoWall()
{
    auto videoWall = createVideoWall();
    resourcePool()->addResource(videoWall);
    return videoWall;
}

nx::Uuid QnResourcePoolTestHelper::addVideoWallItem(const QnVideoWallResourcePtr& videoWall,
    const QnLayoutResourcePtr& itemLayout)
{
    QnVideoWallItem vwitem;
    vwitem.uuid = nx::Uuid::createUuid();

    if (itemLayout)
    {
        itemLayout->setParentId(videoWall->getId());
        vwitem.layout = itemLayout->getId();
    }

    videoWall->items()->addItem(vwitem);
    return vwitem.uuid;
}

bool QnResourcePoolTestHelper::changeVideoWallItem(
    const QnVideoWallResourcePtr& videoWall,
    const nx::Uuid& itemId,
    const QnLayoutResourcePtr& itemLayout)
{
    auto vwitem = videoWall->items()->getItem(itemId);
    if (vwitem.uuid.isNull())
        return false;

    if (itemLayout)
    {
        itemLayout->setParentId(videoWall->getId());
        vwitem.layout = itemLayout->getId();
    }
    else
    {
        vwitem.layout = {};
    }

    videoWall->items()->addOrUpdateItem(vwitem);
    return true;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayoutForVideoWall(
    const QnVideoWallResourcePtr& videoWall)
{
    auto layout = createLayout();
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);
    addVideoWallItem(videoWall, layout);
    return layout;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::createServer(nx::Uuid id)
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(id);
    server->setUrl("http://localhost:7001");
    return server;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer(ServerFlags additionalFlags)
{
    auto server = createServer();
    resourcePool()->addResource(server);
    server->setStatus(ResourceStatus::online);
    server->setServerFlags(server->getServerFlags() | additionalFlags);
    return server;
}

QnStorageResourcePtr QnResourcePoolTestHelper::addStorage(const QnMediaServerResourcePtr& server)
{
    QnStorageResourcePtr storage(new nx::StorageResourceStub());
    storage->setParentId(server->getId());
    resourcePool()->addResource(storage);
    return storage;
}

UserGroupData QnResourcePoolTestHelper::createUserGroup(
    QString name,
    Ids parentGroupIds,
    const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights,
    GlobalPermissions permissions)
{
    UserGroupData group{nx::Uuid::createUuid(), name, permissions, parentGroupIds.data};
    group.resourceAccessRights = resourceAccessRights;
    addOrUpdateUserGroup(group);
    return group;
}

void QnResourcePoolTestHelper::addOrUpdateUserGroup(const UserGroupData& group)
{
    systemContext()->userGroupManager()->addOrUpdate(group);
    systemContext()->accessRightsManager()->setOwnResourceAccessMap(
        group.id, {group.resourceAccessRights.begin(), group.resourceAccessRights.end()});
}

void QnResourcePoolTestHelper::removeUserGroup(const nx::Uuid& groupId)
{
    systemContext()->userGroupManager()->remove(groupId);
}

void QnResourcePoolTestHelper::clear()
{
    using namespace nx::vms::common;

    systemContext()->accessRightsManager()->resetAccessRights({});
    resourcePool()->clear(/*notify*/ true);
    systemContext()->showreelManager()->resetShowreels();

    // We cannot use `resetAll` method as `NonEditableUsersAndGroups` is connected to the `reset`
    // signal via queued connection. Thus groups are not actually removed from cache.
    // systemContext()->userGroupManager()->resetAll({});
    auto groups = systemContext()->userGroupManager()->ids(UserGroupManager::Selection::custom);
    for (auto groupId: groups)
        systemContext()->userGroupManager()->remove(groupId);
}

nx::vms::common::AnalyticsPluginResourcePtr QnResourcePoolTestHelper::addAnalyticsIntegration(
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    using namespace nx::vms::common;

    AnalyticsPluginResourcePtr integration(new AnalyticsPluginResource());
    integration->setIdUnsafe(nx::Uuid::createUuid());
    integration->setTypeId(nx::vms::api::AnalyticsPluginData::kResourceTypeId);
    resourcePool()->addResource(integration);

    auto engine = AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
    engine->setIdUnsafe(nx::Uuid::createUuid());
    engine->setTypeId(nx::vms::api::AnalyticsEngineData::kResourceTypeId);
    engine->setParentId(integration->getId());
    resourcePool()->addResource(engine);

    engine->setManifest(manifest);

    return integration;
}
