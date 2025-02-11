// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_parameters.h>

namespace nx::vms::event {

namespace {

static const std::set<EventType> kAllEvents{
    EventType::cameraMotionEvent,
    EventType::cameraInputEvent,
    EventType::cameraDisconnectEvent,
    EventType::storageFailureEvent,
    EventType::networkIssueEvent,
    EventType::cameraIpConflictEvent,
    EventType::serverFailureEvent,
    EventType::serverConflictEvent,
    EventType::serverStartEvent,
    EventType::ldapSyncIssueEvent,
    EventType::licenseIssueEvent,
    EventType::saasIssueEvent,
    EventType::backupFinishedEvent,
    EventType::poeOverBudgetEvent,
    EventType::fanErrorEvent,
    EventType::softwareTriggerEvent,
    EventType::analyticsSdkEvent,
    EventType::analyticsSdkObjectDetected,
    EventType::pluginDiagnosticEvent,
    EventType::serverCertificateError,
    EventType::userDefinedEvent
};

/**
 * User should not be able to create these events, but should be able to view them in the event
 * log.
 */
static const std::set<EventType> kDeprecatedEvents{
    EventType::backupFinishedEvent
};

} // namespace

bool isNonDeprecatedEvent(EventType eventType)
{
    return !kDeprecatedEvents.contains(eventType);
}

bool isSiteHealth(EventType eventType)
{
    return eventType >= EventType::siteHealthEvent && eventType <= EventType::maxSiteHealthEvent;
}

QList<EventType> allEvents(const EventTypePredicateList& predicates)
{
    QList<EventType> result;
    for (const auto eventType: kAllEvents)
    {
        if (std::all_of(predicates.cbegin(), predicates.cend(),
            [eventType](const auto& predicate){ return predicate(eventType); }))
        {
            result.push_back(eventType);
        }
    }
    return result;
}

bool isResourceRequired(EventType eventType)
{
    return requiresCameraResource(eventType)
        || requiresServerResource(eventType);
}

// Check if camera required for this event to setup a rule. Camera selector will be displayed.
bool requiresCameraResource(EventType eventType)
{
    switch (eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::cameraDisconnectEvent: //< Think about moving out disconnect event.
        case EventType::softwareTriggerEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
        case EventType::pluginDiagnosticEvent:
            return true;

        default:
            return false;
    }
}

// Check if server required for this event to setup a rule. Server selector will be displayed.
bool requiresServerResource(EventType eventType)
{
    switch (eventType)
    {
        case EventType::poeOverBudgetEvent:
        case EventType::fanErrorEvent:
            return true;
        default:
            return false;
    }
}

std::optional<QnResourceList> sourceResources(
    const EventParameters& params,
    const QnResourcePool* resourcePool,
    const std::function<void(const QString&)>& notFound)
{
    if (params.eventResourceId.isNull() && params.metadata.cameraRefs.empty())
        return std::nullopt;

    QnResourceList result;
    for (const auto& ref: params.metadata.cameraRefs)
    {
        if (auto r = camera_id_helper::findCameraByFlexibleId(resourcePool, ref))
        {
            result.push_back(std::move(r));
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Unable to find event %1 resource ref %2", params.eventType, ref);
            if (notFound)
                notFound(ref);
        }
    }

    if (const auto& id = params.eventResourceId; !id.isNull())
    {
        if (auto r = resourcePool->getResourceById(id))
        {
            result.push_back(std::move(r));
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Unable to find event %1 resource id %2", params.eventType, id);
            if (notFound)
                notFound(id.toSimpleString());
        }
    }

    return result;
}

QStringList splitOnPureKeywords(const QString& keywords)
{
    auto list = nx::utils::smartSplit(keywords, ' ', Qt::SkipEmptyParts);
    for (auto it = list.begin(); it != list.end();)
    {
        if (it->length() >= 2 && it->startsWith('"') && it->endsWith('"'))
        {
            *it = it->mid(1, it->length() - 2);
            if (it->isEmpty())
            {
                it = list.erase(it);
                continue;
            }
        }
        ++it;
    }
    return list;
}

bool checkForKeywords(const QString& value, const QStringList& keywords)
{
    return keywords.isEmpty()
        || (!value.isEmpty()
            && nx::utils::find_if(keywords, [&](const auto& k) { return value.contains(k); }));
}

} // namespace nx::vms::event
