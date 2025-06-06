// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/string.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/client/desktop/system_administration/globals/user_settings_global.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

namespace nx::vms::client::desktop {

static inline int userTypeSortOrder(nx::vms::api::UserType userType)
{
    switch (userType)
    {
        case nx::vms::api::UserType::local:
            return 0;
        case nx::vms::api::UserType::temporaryLocal:
            return 1;
        case nx::vms::api::UserType::cloud:
            return 2;
        case nx::vms::api::UserType::ldap:
            return 3;
    }

    NX_ASSERT(false, "Invalid user type %1", userType);
    return -1;
}

static inline int userTypeSortOrder(UserSettingsGlobal::UserType userType)
{
    switch (userType)
    {
        case UserSettingsGlobal::LocalUser:
            return 0;
        case UserSettingsGlobal::TemporaryUser:
            return 1;
        case UserSettingsGlobal::OrganizationUser:
            return 2;
        case UserSettingsGlobal::ChannelPartnerUser:
            return 3;
        case UserSettingsGlobal::CloudUser:
            return 4;
        case UserSettingsGlobal::LdapUser:
            return 5;
    }

    NX_ASSERT(false, "Invalid user type %1", userType);
    return -1;
}

/**
 * Adds operator< to Derived based on sorting order for users and groups. Requires to implement
 * certain functions in Derived class:
 *
 *   nx::Uuid id() const;
 *   bool isGroup() const;
 *   nx::vms::api::UserType userType() const;
 *   QString name() const;
 */
template <class Derived>
struct ComparableMember
{
    bool operator<(const Derived& other) const
    {
        const auto self = static_cast<const Derived*>(this);

        const bool leftIsGroup = self->isGroup();
        const bool rightIsGroup = other.isGroup();

        // Users go first.
        if (leftIsGroup != rightIsGroup)
            return rightIsGroup;

        const auto leftId = self->id();
        const auto rightId = other.id();

        // Predefined groups go first.
        const bool predefinedLeft = nx::vms::common::PredefinedUserGroups::contains(leftId);
        const bool predefinedRight = nx::vms::common::PredefinedUserGroups::contains(rightId);
        if (predefinedLeft != predefinedRight)
            return predefinedLeft;
        else if (predefinedLeft)
            return leftId < rightId;

        // Sort according to type.
        const auto leftUserType = self->userType();
        const auto rightUserType = other.userType();
        if (leftUserType != rightUserType)
            return userTypeSortOrder(leftUserType) < userTypeSortOrder(rightUserType);

        // "LDAP Default" goes in front of all LDAP groups.
        if (leftId == nx::vms::api::kDefaultLdapGroupId
            || rightId == nx::vms::api::kDefaultLdapGroupId)
        {
            // When doing a search, both l and r can be kDefaultLdapGroupId.
            return leftId == nx::vms::api::kDefaultLdapGroupId
                && rightId != nx::vms::api::kDefaultLdapGroupId;
        }

        // Case Insensitive sort.
        const int ret = nx::utils::naturalStringCompare(
            self->name(), other.name(), Qt::CaseInsensitive);

        // Sort identical names by UUID.
        if (ret == 0)
            return leftId < rightId;

        return ret < 0;
    }
};

class ComparableGroup: public ComparableMember<ComparableGroup>
{
    const nx::vms::api::UserGroupData& group;

public:
    ComparableGroup(const nx::vms::api::UserGroupData& group): group(group)
    {
    }

    nx::Uuid id() const { return group.id; }
    bool isGroup() const { return true; }
    nx::vms::api::UserType userType() const { return group.type; }
    QString name() const { return group.name; }
};

} // namespace nx::vms::client::desktop
