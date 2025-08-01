// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_model_data.h"

#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_search/utils/event_data.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/event_log.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop {

EventModelData::EventModelData(const nx::vms::api::rules::EventLogRecord& record):
    m_record(record)
{}

const nx::vms::api::rules::EventLogRecord& EventModelData::record() const
{
    return m_record;
}

nx::vms::rules::AggregatedEventPtr EventModelData::event(core::SystemContext* context) const
{
    if (!m_event)
    {
        m_event = nx::vms::rules::AggregatedEventPtr::create(
            context->vmsRulesEngine(),
            m_record);
    }

    NX_ASSERT(m_event);
    return m_event;
}

const QVariantMap& EventModelData::details(SystemContext* context) const
{
    const auto detailLevel = appContext()->localSettings()->resourceInfoLevel();
    return event(context)->details(context, detailLevel);
}

QString EventModelData::eventType() const
{
    if (m_event)
        return m_event->type();

    return m_record.eventData.value(nx::vms::rules::utils::kType).toString();
}

nx::vms::rules::ActionPtr EventLogModelData::action(SystemContext* context) const
{
    if (!m_action)
        m_action = context->vmsRulesEngine()->buildAction(m_record.actionData);

    NX_ASSERT(m_action);
    return m_action;
}

QString EventLogModelData::actionType() const
{
    if (m_action)
        return m_action->type();

    return m_record.actionData.value(nx::vms::rules::utils::kType).toString();
}

void EventLogModelData::setCompareString(const QString& str)
{
    m_compareString = str;
}

const QString& EventLogModelData::compareString() const
{
    return m_compareString;
}

QString EventLogModelData::eventTitle(SystemContext* context) const
{
    return nx::vms::rules::utils::EventLog::caption(details(context));
}

const QVariantMap& EventLogModelData::actionDetails(SystemContext* context) const
{
    if (m_actionDetails.empty())
        m_actionDetails = action(context)->details(context);

    return m_actionDetails;
}

} // namespace nx::vms::client::desktop
