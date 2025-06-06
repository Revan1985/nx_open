// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtGui/QBrush>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors_fwd.h>
#include <nx/vms/api/rules/event_log_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class EventLogModelData;

class EventLogModel:
    public QAbstractItemModel,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QAbstractItemModel;

public:
    enum Column
    {
        DateTimeColumn,
        EventColumn,
        EventSourceColumn,
        ActionColumn,
        ActionCameraColumn,
        DescriptionColumn,
        ColumnCount
    };

    EventLogModel(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~EventLogModel();

    void setEvents(nx::vms::api::rules::EventLogRecordList&& records);

    QList<Column> columns() const;
    void setColumns(const QList<Column>& columns);

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual void sort(int column, Qt::SortOrder order) override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /** United list of all event resources, which can be opened on the scene (devices, servers). */
    QnResourceList resourcesForPlayback(const QModelIndex& index) const;

    QString eventType(int row) const;
    nx::vms::api::analytics::EventTypeId analyticsEventType(int row) const;
    std::chrono::milliseconds eventTimestamp(int row) const;
    QString actionType(int row) const;

    void clear();

private:
    class DataIndex;

    QnResourcePtr resourceData(Column column, const EventLogModelData& data) const;
    QString motionUrl(Column column, const EventLogModelData& data) const;

    QVariant foregroundData(Column column, const EventLogModelData& data) const;
    QVariant iconData(Column column, const EventLogModelData& data) const;
    QVariant mouseCursorData(Column column, const EventLogModelData& data) const;
    QString textData(Column column, const EventLogModelData& data) const;
    QString htmlData(Column column, const EventLogModelData& data) const;
    QString tooltip(Column column, const EventLogModelData& data) const;
    QString description(const EventLogModelData& data) const;

private:
    QList<Column> m_columns;
    QBrush m_linkBrush;
    std::unique_ptr<DataIndex> m_index;
};

} // namespace nx::vms::client::desktop
