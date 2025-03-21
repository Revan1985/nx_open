// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include "field_validator.h"

class QDateTime;

namespace nx::vms::rules {

class ActionBuilder;
class EventFilter;
class Engine;

/**
 * VMS rule description.
 */
class NX_VMS_RULES_API Rule: public QObject
{
    Q_OBJECT

public:
    explicit Rule(nx::Uuid id, const Engine* engine);
    virtual ~Rule() override;

    nx::Uuid id() const;
    void setId(nx::Uuid id);

    QString author() const;
    void setAuthor(QString author);

    const Engine* engine() const;

    // Takes ownership.
    void addEventFilter(std::unique_ptr<EventFilter> filter);
    void insertEventFilter(int index, std::unique_ptr<EventFilter> filter);

    std::unique_ptr<EventFilter> takeEventFilter(int idx);

    QList<EventFilter*> eventFilters();
    QList<const EventFilter*> eventFilters() const;
    QList<const EventFilter*> eventFiltersByType(const QString& type) const;

    // Takes ownership.
    void addActionBuilder(std::unique_ptr<ActionBuilder> builder);
    void insertActionBuilder(int index, std::unique_ptr<ActionBuilder> builder);

    std::unique_ptr<ActionBuilder> takeActionBuilder(int idx);

    // TODO: #amalov Add const-correct builder access.
    QList<ActionBuilder*> actionBuilders() const;
    QList<const ActionBuilder*> actionBuildersByType(const QString& type) const;

    void setComment(const QString& comment);
    QString comment() const;

    void setEnabled(bool isEnabled);
    bool enabled() const;

    bool isInternal() const;
    void setInternal(bool internal);

    void setSchedule(const QByteArray& schedule);
    const QByteArray& schedule() const;
    bool timeInSchedule(QDateTime time) const;

    // Returns whether at least one filter and one builder are added.
    bool isCompleted() const;

    // Returns whether the rule is completed and all the filters are compatible with all the builders.
    bool isCompatible() const;

    using EventFieldFilter = std::function<bool(const EventFilterField*)>;
    using ActionFieldFilter = std::function<bool(const ActionBuilderField*)>;
    ValidationResult validity(
        common::SystemContext* context,
        const EventFieldFilter& eventFieldFilter = {},
        const ActionFieldFilter& actionFieldFilter = {}) const;

private:
    nx::Uuid m_id;
    QString m_author; //< Name of the user who created or edited the rule.
    const Engine* m_engine;

    // TODO: #spanasenko and-or logic
    std::vector<std::unique_ptr<EventFilter>> m_filters;
    std::vector<std::unique_ptr<ActionBuilder>> m_builders;

    QString m_comment;
    bool m_enabled = true;
    bool m_internal = false;
    QByteArray m_schedule;
};

} // namespace nx::vms::rules
