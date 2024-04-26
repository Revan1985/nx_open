// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<NotificationAction>(),
        .displayName = tr("Show Desktop Notification"),
        .description = "",
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::cloud},
        .fields = {
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                tr("To"),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = false,
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .allowEmptySelection = false,
                    .validationPolicy = kBookmarkManagementValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<ActionFlagField>(utils::kAcknowledgeFieldName, tr("Force Acknowledgement")),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),

            makeFieldDescriptor<TextWithFields>("caption", tr("Caption"), QString(),
                {
                    { "text", "{@EventCaption}" },
                    { "visible", false }
                }),
            makeFieldDescriptor<TextWithFields>("description", tr("Description"), QString(),
                {
                    { "text", "{@EventDescription}" },
                    { "visible", false }
                }),
            makeFieldDescriptor<TextWithFields>("tooltip", tr("Tooltip"), QString(),
                {
                    { "text", "{@ExtendedEventDescription}" },
                    { "visible", false }
                }),
            makeFieldDescriptor<EventDevicesField>(utils::kDeviceIdsFieldName, "Event devices"),
            utils::makeExtractDetailFieldDescriptor("sourceName", utils::kSourceNameDetailName),
            utils::makeExtractDetailFieldDescriptor("level", utils::kLevelDetailName),
            utils::makeExtractDetailFieldDescriptor("icon", utils::kIconDetailName),
            utils::makeExtractDetailFieldDescriptor("customIcon", utils::kCustomIconDetailName),
            utils::makeExtractDetailFieldDescriptor("clientAction", utils::kClientActionDetailName),
            utils::makeExtractDetailFieldDescriptor("url", utils::kUrlDetailName),
            utils::makeExtractDetailFieldDescriptor("extendedCaption", utils::kExtendedCaptionDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device}},
            {utils::kServerIdFieldName, {ResourceType::server}},
        },
    };
    return kDescriptor;
}

QVariantMap NotificationAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName, utils::StringHelper(context).subjects(users()));

    return result;
}

} // namespace nx::vms::rules
