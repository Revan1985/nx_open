// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QHash>

#include <core/resource/layout_resource.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <recording/time_period.h>

#include "resource_fwd.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API LayoutResource: public QnLayoutResource
{
    Q_OBJECT

public:
    using DataHash = QHash<int, QVariant>;
    using ItemsRemapHash = QHash<nx::Uuid, nx::Uuid>;

    static qreal cellSpacingValue(CellSpacing spacing);
    static bool isEqualCellSpacing(CellSpacing spacing, qreal value);

    LayoutResource();

    void setPredefinedCellSpacing(CellSpacing spacing);

    /**
     * Clone provided items list. Ids will be re-generated, zoom links correspondingly fixed.
     * @param items Source layout items.
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    static common::LayoutItemDataList cloneItems(
        common::LayoutItemDataList items, ItemsRemapHash* remapHash = nullptr);

    /**
     * Clone items of the current layout to the target layout. Ids of the layout items will be
     *     re-generated, zoom links correspondingly fixed.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneItems(LayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the target one.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneTo(LayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the new layout.
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     * @return New layout (not added to any system context).
     */
    LayoutResourcePtr clone(ItemsRemapHash* remapHash = nullptr) const;

    nx::vms::api::LayoutData snapshot() const;
    void storeSnapshot();
    void updateSnapshot(const QnLayoutResourcePtr& remoteLayout);
    void restoreFromSnapshot();

    /**
     * Initiate saving current layout to the server.
     * Calls virtual doSaveAsync() to perform actual saving.
     * @param callback Callback with operation result to be called when the operation finishes.
     * @return True if asynchronous save is started, or false if it failed to start.
     */
    using SaveLayoutResultFunction = std::function<void(
        bool /*success*/,
        const LayoutResourcePtr& /*layout*/)>;
    bool saveAsync(SaveLayoutResultFunction callback = SaveLayoutResultFunction());

    /** Checks whether the layout is changed. */
    bool isChanged() const;

    /** Checks whether the layout is saveable, changed and not being saved. */
    bool canBeSaved() const;

    /** Allowed time period range. Can be applied e.g for exported layout or for audit trail. */
    QnTimePeriod localRange() const;

    /** Set allowed time period range for the layout. */
    void setLocalRange(const QnTimePeriod& value);

    /** Runtime layout data. */
    DataHash data() const;

    /** Set runtime layout data. */
    void setData(const DataHash& data);

    /** Runtime layout data. */
    QVariant data(int role) const;

    /** Set runtime layout data. */
    void setData(int role, const QVariant &value);

    /** Runtime data per item. */
    QVariant itemData(const nx::Uuid& id, int role) const;

    /** Store runtime data. Emits itemDataChanged if data differs from existing. */
    void setItemData(const nx::Uuid& id, int role, const QVariant& data);

    /** Remove all runtime data for the given item. Does not emit signals. */
    void cleanupItemData(const nx::Uuid& id);

    /** Remove all runtime data for all items. Does not emit signals. */
    void cleanupItemData();

    /**
     * Whether this Layout is a Cross-System Layout. Such layouts are stored in the Cloud and can
     * contain Cameras from different Systems. Cross-System Layouts are personal and cannot be
     * shared.
     **/
    bool isCrossSystem() const;

    enum class LayoutType
    {
        invalid = -1,
        unknown, //< Not yet known, because parent resource hasn't been loaded yet.
        local, //< A private layout of some user.
        shared,
        videoWall, //< Not necessarily a videowall item; just a layout with a videowall parent.
        intercom,
        file //< Exported layout.
    };
    Q_ENUM(LayoutType);

    /**
     * Returns layout type, based on its parent resource, even after the parent resource is removed
     * from the resource pool.
     */
    LayoutType layoutType() const;

    /** Returns the persistent state of this layout, i.e. its snapshot. */
    virtual QnLayoutResourcePtr storedLayout() const override;

signals:
    void dataChanged(int role);
    void itemDataChanged(const nx::Uuid& id, int role, const QVariant& data);
    void layoutTypeChanged(const LayoutResourcePtr& resource);
    void canBeSavedChanged(const LayoutResourcePtr& resource);
    void layoutAboutToBeSaved(const LayoutResourcePtr& resource, QPrivateSignal);
    void layoutSaved(bool success, const LayoutResourcePtr& resource, QPrivateSignal);
    void layoutRestored(const LayoutResourcePtr& resource, QPrivateSignal); //< From snapshot.

protected:
    /** Virtual constructor for cloning. */
    virtual LayoutResourcePtr create() const;

    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;
    virtual void handleLayoutChange();

    virtual bool doSaveAsync(SaveLayoutResultFunction callback);

private:
    /** @return Whether data value was changed. */
    bool setItemDataUnderLock(const nx::Uuid& id, int role, const QVariant& data);

    LayoutType calculateLayoutType() const;
    void setLayoutType(LayoutType value);
    void updateLayoutType();

private:
    QnTimePeriod m_localRange;
    DataHash m_data;
    QHash<nx::Uuid, DataHash> m_itemData;

    /** Saved state of the layout, which can be used to rollback unapplied changes. */
    nx::vms::api::LayoutData m_snapshot;
    mutable QnLayoutResourcePtr m_snapshotLayout;

    std::atomic<LayoutType> m_layoutType{LayoutType::unknown};
    nx::utils::ScopedConnection m_resourcePoolConnection;

    std::atomic_bool m_changed = false;
    std::atomic_bool m_beingSaved = false;
};

} // namespace nx::vms::client::core
