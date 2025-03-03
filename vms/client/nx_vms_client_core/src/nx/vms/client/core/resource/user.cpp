// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user.h"

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/global_permission_deprecated.h>
#include <nx/vms/client/core/system_context.h>

using namespace nx::vms::api;

namespace nx::vms::client::core {

UserResource::UserResource(nx::vms::api::UserType userType, nx::vms::api::UserExternalId externalId):
    QnUserResource(userType, externalId)
{
    connect(this, &QnUserResource::userGroupsChanged, this,
        [this]()
        {
            std::unordered_set<nx::Uuid> externalOrgGroupIds;
            for (const auto& id: orgGroupIds())
            {
                externalOrgGroupIds.insert(id);
                if(!m_externalToLocalOrgGroupIds.contains(id))
                    m_externalToLocalOrgGroupIds[id] = nx::Uuid::createUuid();
            }

            std::erase_if(
                m_externalToLocalOrgGroupIds,
                [&externalOrgGroupIds](const auto& pair)
                {
                    return !externalOrgGroupIds.contains(pair.first);
                });

            m_localOrgGroupIds.clear();
            m_localOrgGroupIds.reserve(m_externalToLocalOrgGroupIds.size());
            for (const auto& [external, local]: m_externalToLocalOrgGroupIds)
                m_localOrgGroupIds.push_back(local);
        });
}

UserResource::UserResource(UserModelV1 data):
    UserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);

    GlobalPermissions permissions;
    std::vector<nx::Uuid> groupIds;
    std::map<nx::Uuid, AccessRights> resourceAccessRights;
    std::tie(permissions, groupIds, resourceAccessRights) =
        migrateAccessRights(
            data.permissions,
            data.accessibleResources.value_or(std::vector<nx::Uuid>()),
            data.isOwner);

    if (!data.userRoleId.isNull())
        groupIds.push_back(data.userRoleId);

    setRawPermissions(permissions);
    setSiteGroupIds(groupIds);
    setResourceAccessRights(resourceAccessRights);

    m_overwrittenData = std::move(data);
}

UserResource::UserResource(UserModelV3 data):
    UserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);
    setAttributes(data.attributes);
    setRawPermissions(data.permissions);
    setSiteGroupIds(data.groupIds);
    if (data.orgGroupIds)
        setOrgGroupIds(*data.orgGroupIds);
    setResourceAccessRights(data.resourceAccessRights);
}

void UserResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    // Ignore user data changes in the compatibility mode connection.
    if (m_overwrittenData)
        return;

    base_type::updateInternal(source, notifiers);
}

rest::Handle UserResource::saveSettings(
    const UserSettings& value,
    std::function<void(bool /*success*/, rest::Handle /*handle*/)> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto systemContext = SystemContext::fromResource(toSharedPointer());
    if (!NX_ASSERT(systemContext))
        return rest::Handle{};

    const auto backup = settings();
    setSettings(value);

    auto internalCallback = nx::utils::guarded(this,
        [this, executor, callback, backup](
            bool success,
            rest::Handle handle,
            rest::ErrorOrData<nx::vms::api::UserModelV3> /*errorOrData*/)
        {
            if (!success)
                setSettings(backup);

            if (callback)
                executor.submit(callback, success, handle);
        });

    auto api = systemContext->connectedServerApi();
    if (!api)
        return rest::Handle{};

    return api->patchUserSettings(
        getId(),
        value,
        internalCallback,
        thread());
}

void UserResource::setSettings(const UserSettings& settings)
{
    setProperty(ResourcePropertyKey::User::kUserSettings,
        QString::fromStdString(nx::reflect::json::serialize(settings)));
}

UserSettings UserResource::settings() const
{
    const auto value = getProperty(ResourcePropertyKey::User::kUserSettings);
    UserSettings result;
    nx::reflect::json::deserialize<UserSettings>(value.toStdString(), &result);

    return result;
}

bool UserResource::shouldMaskUser() const
{
    if (!base_type::shouldMaskUser())
        return false;

    auto systemContext = SystemContext::fromResource(toSharedPointer());
    if (!NX_ASSERT(systemContext))
        return false;

    return systemContext->user().get() != this;
}

std::map<nx::Uuid, nx::Uuid> UserResource::mappedOrgGroupIds() const
{
    return m_externalToLocalOrgGroupIds;
}

std::vector<nx::Uuid> UserResource::siteAndMappedOrgGroupIds() const
{
    auto groups = siteGroupIds();
    groups.reserve(groups.size() + m_localOrgGroupIds.size());
    groups.insert(groups.end(), m_localOrgGroupIds.cbegin(), m_localOrgGroupIds.cend());
    return groups;
}

} // namespace nx::vms::client::core
