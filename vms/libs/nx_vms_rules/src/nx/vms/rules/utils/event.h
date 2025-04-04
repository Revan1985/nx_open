// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"

namespace nx::vms::api { struct UserSettings; }

namespace nx::vms::rules {

inline bool isProlonged(const EventPtr& event)
{
    const auto eventState = event->state();
    return eventState == State::started || eventState == State::stopped;
}

inline bool isInstant(const EventPtr& event)
{
    return event->state() == State::instant;
}

NX_VMS_RULES_API bool hasSourceCamera(const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API bool hasSourceServer(const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API bool hasSourceUser(const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API std::string getDeviceField(const vms::rules::ItemDescriptor& eventDescriptor);

enum class EventDurationType
{
    instant,
    prolonged,
    mixed
};

NX_VMS_RULES_API EventDurationType getEventDurationType(
    const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API EventDurationType getEventDurationType(
    const Engine* engine, const EventFilter* eventFilter);

NX_VMS_RULES_API QSet<State> getPossibleFilterStatesForEventDescriptor(
    const vms::rules::ItemDescriptor& eventDescriptor);

NX_VMS_RULES_API QSet<State> getPossibleFilterStatesForEventFilter(
    const Engine* engine, const EventFilter* eventFilter);

NX_VMS_RULES_API bool isEventWatched(const nx::vms::api::UserSettings& settings,
    const QString& eventType);

} // namespace nx::vms::rules
