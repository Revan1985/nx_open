// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recursive_members_model.h"

#include <QtQml/QtQml>

#include <core/resource_access/subject_hierarchy.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

RecursiveMembersModel::RecursiveMembersModel(SystemContext* systemContext):
    SystemContextAware(systemContext)
{
}

RecursiveMembersModel::RecursiveMembersModel():
    SystemContextAware(appContext()->currentSystemContext())
{
}

RecursiveMembersModel::~RecursiveMembersModel()
{
}

QHash<int, QByteArray> RecursiveMembersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "text";
    roles[IdRole] = "id";
    roles[IsUserRole] = "isUser";
    roles[OffsetRole] = "offset";
    roles[IsLdap] = "isLdap";
    roles[IsTemporary] = "isTemporary";
    roles[CanEditParents] = "canEditParents";
    roles[UserType] = "userType";
    return roles;
}

QVariant RecursiveMembersModel::getMemberData(int offset, const nx::Uuid& id, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return m_cache->info(id).name;
        case IsUserRole:
            return !m_cache->info(id).isGroup;
        case OffsetRole:
            return offset;
        case IdRole:
            return QVariant::fromValue(id);
        case IsLdap:
            return m_cache->info(id).userType == UserSettingsGlobal::LdapUser;
        case IsTemporary:
            return m_cache->info(id).userType == UserSettingsGlobal::TemporaryUser;
        case CanEditParents:
            return false;
        case UserType:
            return m_cache->info(id).userType;
    }
    return {};
}

QVariant RecursiveMembersModel::getData(int offset, const nx::Uuid& groupId, int row, int role) const
{
    if (row == -1)
        return {};

    if (row == 0)
        return getMemberData(offset, groupId, role);

    const auto members = m_cache->sorted(groupId);

    if (row > 0 && (qsizetype) row <= members.users.size())
        return getMemberData(offset + 1, members.users[row - 1], role);

    const int groupRow = row - members.users.size() - 1;

    const auto [subGroupId, subGroupRow] = findGroupRow(members.groups, groupRow);

    return getData(offset + 1, subGroupId, subGroupRow, role);
}

/**
 * Translate a row within groups into a row within a subgroup.
 * Consider the following recursive structure:
 *
 *     group1
 *         user1
 *     group2
 *         user2
 *         user3
 *
 * Then for the arguments (groups = [group1, group2], row = 3) the result would be <group2, 1>.
 */
std::pair<nx::Uuid, int> RecursiveMembersModel::findGroupRow(
    const QList<nx::Uuid>& groups,
    int row) const
{
    int currentRow = 0;

    for (const nx::Uuid& groupId: groups)
    {
        const int itemsInGroup = 1 + m_totalSubItems.value(groupId, 0);
        const int rowOffsetInGroup = row - currentRow;

        if (rowOffsetInGroup >= 0 && rowOffsetInGroup < itemsInGroup)
            return {groupId, rowOffsetInGroup};

        currentRow += itemsInGroup;
    }

    return {{}, -1};
}

QVariant RecursiveMembersModel::data(const QModelIndex& index, int role) const
{
    static constexpr int kInitialOffset = 1;

    if (index.row() < 0 || index.row() >= rowCount() || !m_cache)
        return {};

    const auto members = m_cache->sorted(m_groupId);

    if (index.row() < members.users.size())
    {
        const auto userId = members.users.at(index.row());

        return getMemberData(kInitialOffset, userId, role);
    }

    const auto [subGroupId, subGroupRow] =
        findGroupRow(members.groups, index.row() - members.users.size());

    if (subGroupRow == -1)
        return {};

    return getData(kInitialOffset, subGroupId, subGroupRow, role);
}

int RecursiveMembersModel::rowCount(const QModelIndex&) const
{
    return m_itemCount;
}

void RecursiveMembersModel::registerQmlType()
{
    qmlRegisterType<RecursiveMembersModel>("nx.vms.client.desktop", 1, 0, "RecursiveMembersModel");
}

void RecursiveMembersModel::setGroupId(const nx::Uuid& groupId)
{
    if (m_groupId == groupId)
        return;

    m_groupId = groupId;
    emit groupIdChanged();

    loadData();
}

void RecursiveMembersModel::setMembersCache(MembersCache* cache)
{
    if (m_cache == cache)
        return;

    if (m_cache)
        disconnect(m_cache);

    m_cache = cache;
    emit membersCacheChanged();

    const auto onCacheChanged =
        [this](int, const nx::Uuid&, const nx::Uuid &parentId)
        {
            if (parentId.isNull())
                return; //< Ignore top level updates.

            // Here a subject is either inserted or removed. If the total number of rows did not
            // change then the modification happened outside the scope of current group and we can
            // ignore row updates. However, to calculate the total number of rows we still need to
            // call loadData().
            loadData(/*updateAllRows*/ false);
        };

    connect(cache, &MembersCache::endInsert, this, onCacheChanged);
    connect(cache, &MembersCache::endRemove, this, onCacheChanged);
    connect(cache, &MembersCache::reset, this,
        [this]
        {
            beginResetModel();
            m_itemCount = 0;
            m_totalSubItems.clear();
            endResetModel();
            emit countChanged();
        });

    loadData();
}

void RecursiveMembersModel::loadData(bool updateAllRows)
{
    if (!m_cache || m_groupId.isNull())
    {
        if (m_itemCount > 0)
        {
            beginResetModel();
            m_itemCount = 0;
            m_totalSubItems = {};
            endResetModel();

            emit countChanged();

            if (m_hasCycle)
            {
                m_hasCycle = false;
                emit hasCycleChanged();
            }
        }
        return;
    }

    QHash<nx::Uuid, int> totalSubItems;

    static constexpr int kCycleGuardValue = -1;
    static constexpr int kEmptyValue = -2;

    const std::function<int(nx::Uuid)> subItemsCount =
        [this, &totalSubItems, &subItemsCount](nx::Uuid id) -> int
        {
            const int visitedSubItems = totalSubItems.value(id, kEmptyValue);
            if (visitedSubItems == kCycleGuardValue)
                return kCycleGuardValue;

            if (visitedSubItems >= 0)
                return visitedSubItems;

            totalSubItems[id] = kCycleGuardValue;

            const auto members = m_cache->sorted(id);

            int total = members.users.size();

            for (const auto& subGroupId: members.groups)
            {
                const int subSize = subItemsCount(subGroupId);
                if (subSize == kCycleGuardValue)
                    return kCycleGuardValue; // Found a cycle, the configuration is invalid.

                total += 1 + subSize;
            }

            totalSubItems[id] = total;

            return total;
        };

    // Recursively count the number of model rows.
    const auto members = m_cache->sorted(m_groupId);

    bool hasCycle = false;
    int itemCount = 0;

    int userCount = members.users.size();
    for (const auto& id: members.groups)
    {
        const int addCount = subItemsCount(id);
        if (addCount == kCycleGuardValue)
        {
            hasCycle = true;
            break;
        }

        itemCount += 1 + addCount;
    }

    if (m_hasCycle != hasCycle)
    {
        m_hasCycle = hasCycle;
        emit hasCycleChanged();
    }

    // Hide all groups if cycle is detected.
    itemCount = hasCycle ? userCount : (itemCount + userCount);

    // Notify about changes without resetting the model.
    const int diff = itemCount - m_itemCount;

    if (diff > 0)
    {
        beginInsertRows({}, m_itemCount, m_itemCount + diff - 1);
        m_itemCount = itemCount;
        m_totalSubItems = totalSubItems;
        endInsertRows();
        emit countChanged();
        updateAllRows = true;
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, m_itemCount + diff, m_itemCount - 1);
        m_itemCount = itemCount;
        m_totalSubItems = totalSubItems;
        endRemoveRows();
        emit countChanged();
        updateAllRows = true;
    }
    else
    {
        m_totalSubItems = totalSubItems;
    }

    if (m_itemCount > 0 && updateAllRows)
    {
        emit dataChanged(
            this->index(0, 0),
            this->index(m_itemCount - 1, 0));
    }
}

} // namespace nx::vms::client::desktop
