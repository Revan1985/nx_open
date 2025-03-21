// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data/data_macros.h"
#include "../data/id_data.h"
#include "../data/schedule_task_data.h"
#include "action_builder.h"
#include "event_filter.h"

namespace nx::vms::api::rules {

struct NX_VMS_API Rule: IdData
{
    // TODO: #spanasenko and-or logic
    QList<EventFilter> eventList;
    QList<ActionBuilder> actionList;

    /**%apidoc[opt] Is rule currently enabled. */
    bool enabled = true;
    /**%apidoc[unused] */
    bool internal = false;

    /**%apidoc[opt] Schedule of the rule. Empty list means the rule is always enabled. */
    nx::vms::api::ScheduleTaskDataList schedule;

    /**%apidoc[opt] String comment explaining the rule. */
    QString comment;

    /**%apidoc[unused] User who created or updated the given rule. */
    QString author;
};

#define nx_vms_api_rules_Rule_Fields \
    IdData_Fields(eventList)(actionList)(enabled)(internal)(schedule)(comment)(author)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(Rule, (json)(ubjson))
NX_REFLECTION_INSTRUMENT(Rule, nx_vms_api_rules_Rule_Fields)

struct NX_VMS_API RuleV4: public IdData
{
    /**%apidoc Event filter field data. */
    std::map<QString, QJsonValue> event;

    /**%apidoc Action builder field data. */
    std::map<QString, QJsonValue> action;

    /**%apidoc[opt] Is rule currently enabled. */
    bool enabled = true;

    /**%apidoc[opt] Schedule of the rule. Empty list means the rule is always enabled. */
    nx::vms::api::ScheduleTaskDataList schedule;

    /**%apidoc[opt] String comment explaining the rule. */
    QString comment;
};

#define nx_vms_api_rules_RuleV4_Fields \
    IdData_Fields(event)(action)(enabled)(schedule)(comment)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(RuleV4, (json))
NX_REFLECTION_INSTRUMENT(RuleV4, nx_vms_api_rules_RuleV4_Fields)

// A dummy struct used in ec2 transactions.
struct NX_VMS_API ResetRules
{
    /**%apidoc[opt] Reset to default rule set if true, clear rules if false. */
    bool useDefault = true;

    /**%apidoc[unused] User who triggered given transaction. */
    QString author;
};

#define nx_vms_api_rules_ResetRules_Fields (useDefault)
NX_VMS_API_DECLARE_STRUCT_EX(ResetRules, (json)(ubjson))
NX_REFLECTION_INSTRUMENT(ResetRules, nx_vms_api_rules_ResetRules_Fields)

} // namespace nx::vms::api::rules
