// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/system_administration/models/recursive_members_model.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

namespace test {

class MembersModelTest: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();
        m_model.reset(new MembersModel(systemContext()));
        createData();
    }

    void createData()
    {
        // Build the following structure:
        //
        // "admin",
        // "user1",
        // "user2",
        // "user3",
        // "user4",
        // "user5",
        // "Administrators",
        // "Power Users",
        // "Advanced Viewers",
        // "Viewers",
        // "Live Viewers",
        // "Site Health Viewers",
        // "group1",
        // "    group3",
        // "        user1",
        // "        user2",
        // "        user3",
        // "        group4",
        // "            user5",
        // "            group5",
        // "        group5",
        // "group2",
        // "    user2",
        // "    user3",
        // "    group3",
        // "        user1",
        // "        user2",
        // "        user3",
        // "        group4",
        // "            user5",
        // "            group5",
        // "        group5",
        // "group3",
        // "    user1",
        // "    user2",
        // "    user3",
        // "    group4",
        // "        user5",
        // "        group5",
        // "    group5",
        // "group4",
        // "    user5",
        // "    group5",
        // "group5",

        m_admin = addUser("admin", {api::kAdministratorsGroupId});
        const auto admin =
            systemContext()->resourcePool()->getResourceById<QnUserResource>(m_admin);

        // Login as admin.
        systemContext()->accessController()->setUser(admin);

        auto group1 = addGroup("group1", {});
        m_group2 = addGroup("group2", {});
        m_group3 = addGroup("group3", {group1, m_group2});
        m_group4 = addGroup("group4", {m_group3});
        m_group5 = addGroup("group5", {m_group3, m_group4});

        addUser("user1", {m_group3});
        m_user2 = addUser("user2", {m_group2, m_group3});
        m_user3 = addUser("user3", {m_group2, m_group3});
        addUser("user4", {});
        m_user5 = addUser("user5", {m_group4}, api::UserType::temporaryLocal);
    }

    void verifyRow(int row, const QString& name, int offset)
    {
        ASSERT_EQ(name, m_model->data(m_model->index(row), Qt::DisplayRole).toString());
        ASSERT_EQ(offset, m_model->data(m_model->index(row), MembersModel::OffsetRole).toInt());
    }

    nx::Uuid addUser(
        const QString& name,
        const std::vector<nx::Uuid>& parents,
        const api::UserType& userType = api::UserType::local)
    {
        QnUserResourcePtr user(
            new QnUserResource(userType, /*externalId*/ {}));
        user->setIdUnsafe(nx::Uuid::createUuid());
        user->setName(name);
        user->addFlags(Qn::remote);
        user->setSiteGroupIds(parents);
        systemContext()->resourcePool()->addResource(user);
        return user->getId();
    }

    nx::Uuid addGroup(const QString& name, const std::vector<nx::Uuid>& parents)
    {
        api::UserGroupData group;
        group.id = nx::Uuid::createUuid();
        group.name = name;
        group.parentGroupIds = parents;
        systemContext()->userGroupManager()->addOrUpdate(group);
        return group.id;
    }

    nx::Uuid addLdapDefaultGroup(const std::vector<nx::Uuid>& parents)
    {
        api::UserGroupData group;
        group.id = nx::vms::api::kDefaultLdapGroupId;
        group.name = "LDAP Default";
        group.type = api::UserType::ldap;
        group.parentGroupIds = parents;
        systemContext()->userGroupManager()->addOrUpdate(group);
        return group.id;
    }

    void removeUser(const nx::Uuid& id)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resourcePool->removeResource(resource);
    }

    void removeGroup(const nx::Uuid& id)
    {
        const auto allUsers = systemContext()->resourcePool()->getResources<QnUserResource>();
        for (const auto& user: allUsers)
        {
            std::vector<nx::Uuid> ids = user->siteGroupIds();
            if (nx::utils::erase_if(ids, [&user](auto id){ return id == user->getId(); }))
                user->setSiteGroupIds(ids);
        }

        for (auto group: systemContext()->userGroupManager()->groups())
        {
            const auto isTargetGroup = [&group](auto id){ return id == group.id; };
            if (nx::utils::erase_if(group.parentGroupIds, isTargetGroup))
                systemContext()->userGroupManager()->addOrUpdate(group);
        }

        systemContext()->userGroupManager()->remove(id);
    }

    void renameGroup(const nx::Uuid& id, const QString& newName)
    {
        auto group = systemContext()->userGroupManager()->find(id).value_or(api::UserGroupData{});
        ASSERT_EQ(id, group.id);
        group.name = newName;
        systemContext()->userGroupManager()->addOrUpdate(group);
    }

    void renameUser(const nx::Uuid& id, const QString& newName)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resource->setName(newName);
    }

    void enableUser(const nx::Uuid& id, bool enable)
    {
        auto resourcePool = systemContext()->resourcePool();
        auto resource = resourcePool->getResourceById<QnUserResource>(id);
        ASSERT_TRUE(!resource.isNull());
        resource->setEnabled(enable);
    }

    QStringList visualData(const QAbstractListModel* model, int filterByRole = -1)
    {
        QStringList result;
        for (int row = 0; row < model->rowCount(); row++)
        {
            if (filterByRole != -1 && !model->data(model->index(row), filterByRole).toBool())
                continue;

            auto text = model->data(model->index(row), Qt::DisplayRole).toString();
            auto offset = model->data(model->index(row), MembersModel::OffsetRole).toInt();

            QString offsetStr(4 * offset, ' ');
            QString line = offsetStr + text;
            result << line;
        }
        return result;
    }

    QStringList filterTopLevelByRole(int role)
    {
        QStringList result;
        const auto data = visualData(m_model.get());
        for (int row = 0; row < data.size(); ++row)
        {
            if (m_model->data(m_model->index(row), MembersModel::OffsetRole).toInt() > 0)
                continue;

            if (m_model->data(m_model->index(row), role).toBool())
                result << data.at(row);
        }
        return result;
    }

    void addMember(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData(m_model.get()).indexOf(name));
        ASSERT_FALSE(m_model->data(topLevelIndex, MembersModel::IsMemberRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, true, MembersModel::IsMemberRole));
    }

    void removeMember(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData(m_model.get()).indexOf(name));
        ASSERT_TRUE(m_model->data(topLevelIndex, MembersModel::IsMemberRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, false, MembersModel::IsMemberRole));
    }

    void addParent(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData(m_model.get()).indexOf(name));
        ASSERT_FALSE(m_model->data(topLevelIndex, MembersModel::IsParentRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, true, MembersModel::IsParentRole));
    }

    void removeParent(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData(m_model.get()).indexOf(name));
        ASSERT_TRUE(m_model->data(topLevelIndex, MembersModel::IsParentRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, false, MembersModel::IsParentRole));
    }

    std::unique_ptr<MembersModel> m_model;
    nx::Uuid m_group2;
    nx::Uuid m_group3;
    nx::Uuid m_group4;
    nx::Uuid m_group5;
    nx::Uuid m_admin;
    nx::Uuid m_user2;
    nx::Uuid m_user3;
    nx::Uuid m_user5;
};

TEST_F(MembersModelTest, prerequisiteCheckPredefinedGroupNames)
{
    using nx::vms::common::PredefinedUserGroups;

    ASSERT_EQ(PredefinedUserGroups::find(api::kAdministratorsGroupId)->name, "Administrators");
    ASSERT_EQ(PredefinedUserGroups::find(api::kPowerUsersGroupId)->name, "Power Users");
    ASSERT_EQ(PredefinedUserGroups::find(api::kAdvancedViewersGroupId)->name, "Advanced Viewers");
    ASSERT_EQ(PredefinedUserGroups::find(api::kViewersGroupId)->name, "Viewers");
    ASSERT_EQ(PredefinedUserGroups::find(api::kLiveViewersGroupId)->name, "Live Viewers");
    ASSERT_EQ(PredefinedUserGroups::find(api::kSystemHealthViewersGroupId)->name,
        "Site Health Viewers");
}

TEST_F(MembersModelTest, removeAndAddParentForGroup)
{
    // Remove group4 from group3 and add it back.
    m_model->setGroupId(m_group4);

    static const QStringList kGroupInitial = {
        "group3",
    };

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsParentRole));

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group4);

    removeParent("group3");

    EXPECT_TRUE(visualData(m_model.get(), MembersModel::IsParentRole).isEmpty());
    EXPECT_TRUE(m_model->parentGroups().isEmpty());

    addParent("group3");

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsParentRole));
}

TEST_F(MembersModelTest, removeAndAddMemberGroup)
{
    // Remove group5 from group3 and add it back.
    m_model->setGroupId(m_group3);

    static const QStringList kGroupInitial = {
        "user1",
        "user2",
        "user3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsMemberRole));

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    removeMember("group4");

    static const QStringList kGroupRemoved = {
        "user1",
        "user2",
        "user3",
        "group5",
    };

    EXPECT_EQ(kGroupRemoved, visualData(m_model.get(), MembersModel::IsMemberRole));

    addMember("group4");

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsMemberRole));
}

TEST_F(MembersModelTest, removeAndAddMemberUser)
{
    // Remove user2 from group3 and add it back.
    m_model->setGroupId(m_group3);

    static const QStringList kGroupInitial = {
        "user1",
        "user2",
        "user3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsMemberRole));

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    removeMember("user2");

    static const QStringList kGroupRemoved = {
        "user1",
        "user3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kGroupRemoved, visualData(m_model.get(), MembersModel::IsMemberRole));

    addMember("user2");

    EXPECT_EQ(kGroupInitial, visualData(m_model.get(), MembersModel::IsMemberRole));
}

TEST_F(MembersModelTest, removeAndAddParentForUser)
{
    // Remove user2 from group3 and add it back.
    m_model->setUserId(m_user2);

    static const QStringList kParentsInitial = {
        "group2",
        "group3",
    };

    EXPECT_EQ(kParentsInitial, visualData(m_model.get(), MembersModel::IsParentRole));

    removeParent("group3");

    static const QStringList kParentRemoved = {
        "group2",
    };

    EXPECT_EQ(kParentRemoved, visualData(m_model.get(), MembersModel::IsParentRole));

    addParent("group3");

    EXPECT_EQ(kParentsInitial, visualData(m_model.get(), MembersModel::IsParentRole));
}

TEST_F(MembersModelTest, allowedMembers)
{
    // Check that group4 cannot have members that are already its parents (direct or indirect).
    m_model->setGroupId(m_group4);

    static const QStringList kAllowedMembers = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "group5"
    };

    const QStringList allowedMembers = filterTopLevelByRole(MembersModel::IsAllowedMember);

    ASSERT_EQ(kAllowedMembers, allowedMembers);
}

TEST_F(MembersModelTest, allowedParents)
{
    // Check that group2 cannot have parents that are already its members (direct or indirect)
    // and cannot be a part of Administrators or Power Users because of temporary user member.
    m_model->setGroupId(m_group2);

    addGroup("group6", {api::kPowerUsersGroupId});
    addGroup("group7", {});

    static const QStringList kAllowedParents = {
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group7"
    };

    const QStringList allowedParents = filterTopLevelByRole(MembersModel::IsAllowedParent);

    ASSERT_EQ(kAllowedParents, allowedParents);
}

TEST_F(MembersModelTest, members)
{
    // Verify direct members of group3.
    m_model->setGroupId(m_group3);

    static const QStringList kDirectMembers = {
        "user1",
        "user2",
        "user3",
        "group4",
        "group5",
    };

    const QStringList members = filterTopLevelByRole(MembersModel::IsMemberRole);

    ASSERT_EQ(kDirectMembers, members);
}

TEST_F(MembersModelTest, parents)
{
    // Verify direct parents of group5.
    m_model->setGroupId(m_group5);

    static const QStringList kDirectParents = {
        "group3",
        "group4",
    };

    const QStringList parents = filterTopLevelByRole(MembersModel::IsParentRole);

    ASSERT_EQ(kDirectParents, parents);
}

TEST_F(MembersModelTest, recursiveMembers)
{
    // Verify recursive members of subgroup group3.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    static const QStringList kRecursiveMembersGroup3 = {
        "    user1",
        "    user2",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "    group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3, visualData(&members));
}

TEST_F(MembersModelTest, removeUserResource)
{
    // Remove user5.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    removeUser(m_user5);

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterRemove = {
        "    user1",
        "    user2",
        "    user3",
        "    group4",
        "        group5",
        "    group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterRemove, visualData(&members));
}

TEST_F(MembersModelTest, addUserResource)
{
    // Add user2a to both group3 and group5.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    addUser("user2a", {m_group3, m_group5});

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user2a",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterAdd = {
        "    user1",
        "    user2",
        "    user2a",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "            user2a",
        "    group5",
        "        user2a",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterAdd, visualData(&members));
}

TEST_F(MembersModelTest, addGroup)
{
    // Add group6 to group4.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    addGroup("group6", {m_group4});

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
        "group6",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterAdd = {
        "    user1",
        "    user2",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "        group6",
        "    group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterAdd, visualData(&members));
}

TEST_F(MembersModelTest, removeGroup)
{
    // Remove m_group4.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    removeGroup(m_group4);

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group5",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterRemove = {
        "    user1",
        "    user2",
        "    user3",
        "    group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterRemove, visualData(&members));
}

TEST_F(MembersModelTest, renameGroup)
{
    // Rename m_group4 -> group55.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    renameGroup(m_group4, "group55");

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group5",
        "group55",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterRename = {
        "    user1",
        "    user2",
        "    user3",
        "    group5",
        "    group55",
        "        user5",
        "        group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterRename, visualData(&members));
}

TEST_F(MembersModelTest, renameUser)
{
    // Rename user3 -> user1a.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(m_group3);

    renameUser(m_user3, "user1a");

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user1a",
        "user2",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersGroup3AfterRename = {
        "    user1",
        "    user1a",
        "    user2",
        "    group4",
        "        user5",
        "        group5",
        "    group5",
    };

    EXPECT_EQ(kRecursiveMembersGroup3AfterRename, visualData(&members));
}

TEST_F(MembersModelTest, addRemoveLdapDefaultGroup)
{
    // Add/remove LDAP Default (special autogenerated group) to Viewers.
    m_model->setGroupId(m_group2);

    RecursiveMembersModel members;
    members.setMembersCache(m_model->membersCache());
    members.setGroupId(nx::vms::api::kViewersGroupId);

    addLdapDefaultGroup({api::kViewersGroupId});

    static const QStringList kAllSubjects = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
        "LDAP Default",
    };

    EXPECT_EQ(kAllSubjects, visualData(m_model.get()));

    static const QStringList kRecursiveMembersViewersAfterAdd = {
        "    LDAP Default",
    };

    EXPECT_EQ(kRecursiveMembersViewersAfterAdd, visualData(&members));

    removeGroup(nx::vms::api::kDefaultLdapGroupId);

    static const QStringList kAllSubjectsNoLdapDefault = {
        "admin",
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Administrators",
        "Power Users",
        "Advanced Viewers",
        "Viewers",
        "Live Viewers",
        "Site Health Viewers",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
    };

    EXPECT_EQ(kAllSubjectsNoLdapDefault, visualData(m_model.get()));
}

TEST_F(MembersModelTest, duplicatesNotEditable)
{
    m_model->setGroupId(m_group3);

    static const QStringList kAllowedParentsNoDuplicates = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "group1",
        "group2",
        "group3",
        "group4",
        "group5",
    };

    const QStringList allowedParentsNoDuplicates =
        filterTopLevelByRole(MembersModel::CanEditParents);

    ASSERT_EQ(kAllowedParentsNoDuplicates, allowedParentsNoDuplicates);

    const auto user2Dup = addUser("user2", {m_group3});
    const auto group4Dup = addGroup("group4", {m_group3});

    static const QStringList kAllowedParentsWithDuplicates = {
        "user1",
        // "user2", //< Duplicate exists.
        "user3",
        "user4",
        "user5",
        "group1",
        "group2",
        "group3",
        // "group4", //< Duplicate exists.
        "group5",
    };

    const QStringList allowedParentsWithDuplicates =
        filterTopLevelByRole(MembersModel::CanEditParents);

    ASSERT_EQ(kAllowedParentsWithDuplicates, allowedParentsWithDuplicates);

    enableUser(user2Dup, false);
    renameGroup(group4Dup, "group4dup");

    static const QStringList kAllowedParentsResolvedDuplicate = {
        "user1",
        "user2",
        "user2",
        "user3",
        "user4",
        "user5",
        "group1",
        "group2",
        "group3",
        "group4",
        "group4dup",
        "group5",
    };

    const QStringList allowedParentsResolvedDuplicate =
        filterTopLevelByRole(MembersModel::CanEditParents);

    ASSERT_EQ(kAllowedParentsResolvedDuplicate, allowedParentsResolvedDuplicate);
}

} // namespace test
} // namespace nx::vms::client::desktop
