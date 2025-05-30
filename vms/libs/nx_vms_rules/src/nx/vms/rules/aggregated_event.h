// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "basic_event.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

/**
 * Wrapper around a list of event aggregation information. May be constructed directly from the
 * list of events - or from a serialized aggregation information.
 */
class NX_VMS_RULES_API AggregatedEvent: public QObject
{
    Q_OBJECT

public:
    explicit AggregatedEvent(const EventPtr& event);
    explicit AggregatedEvent(std::vector<EventPtr>&& eventList);

    /** Constructor from the serialized data. */
    AggregatedEvent(const EventPtr& event, nx::vms::api::rules::PropertyMap aggregatedInfo);

    ~AggregatedEvent() override = default;

    /** Returns property with the given name of the initial event. */
    QVariant property(const char* name) const;

    /** Returns initial event type. */
    QString type() const;

    /** Returns initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /** Returns initial event state. */
    State state() const;

    /** Returns initial event details plus aggregated details. */
    QVariantMap details(common::SystemContext* context, Qn::ResourceInfoLevel detailLevel) const;

    using Filter = std::function<EventPtr(const EventPtr&)>;
    /**
     * Filters aggregated events by the given filter condition. If there is no appropriate events
     * nullptr returned.
     */
    AggregatedEventPtr filtered(const Filter& filter) const;

    using SplitKeyFunction = std::function<QString(const EventPtr&)>;
    /**
     * Split all the aggregated events to a list of AggregatedEventPtr using a key from the given
     * function. All the aggregated events are sorted by a timestamp.
     */
    std::vector<AggregatedEventPtr> split(const SplitKeyFunction& splitKeyFunction) const;

    size_t count() const;

    EventPtr initialEvent() const;

    const std::vector<EventPtr>& aggregatedEvents() const;

    nx::Uuid ruleId() const;
    void setRuleId(nx::Uuid ruleId);

private:
    std::vector<EventPtr> m_aggregatedEvents;
    nx::Uuid m_ruleId;
    std::optional<nx::vms::api::rules::PropertyMap> m_aggregatedInfo;

    /** Cache of the details method call. */
    mutable std::map<Qn::ResourceInfoLevel, QVariantMap> m_detailsCache;

    AggregatedEvent() = default;
};

} // namespace nx::vms::rules
