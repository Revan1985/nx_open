// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_email_action.h"

#include <core/resource/user_resource.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>
#include <utils/email/email.h>

#include "../action_builder_fields/email_message_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/text_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/resource.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SendEmailAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SendEmailAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Send Email")),
        .description = {},
        .flags = {ItemFlag::instant, ItemFlag::eventPermissions},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::cloud},
        .fields = {
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .validationPolicy = kUserWithEmailValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<ActionTextField>(
                utils::kEmailsFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Additional Recipients")),
                /*description*/ {},
                ActionTextFieldProperties{
                    .validationPolicy = kEmailsValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<EmailMessageField>(
                utils::kMessageFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Email Message")),
                {},
                FieldProperties{.visible = false}.toVariantMap()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {{utils::kUsersFieldName, {ResourceType::user}}},
    };
    return kDescriptor;
}

QSet<QString> SendEmailAction::emailAddresses(
    common::SystemContext* context, bool activeOnly, bool displayOnly) const
{
    QSet<QString> recipients;

    const auto insertRecipientIfValid =
        [&recipients](const QString& recipient)
    {
        const auto simplified = recipient.trimmed().toLower();
        if (simplified.isEmpty())
            return;

        const QnEmailAddress address{simplified};
        if (address.isValid())
            recipients.insert(address.value());
    };

    for (const auto& user: utils::users(users(), context, activeOnly))
        insertRecipientIfValid(displayOnly ? user->displayEmail() : user->getEmail());

    for (const auto& additionalRecipient: emails().split(';', Qt::SkipEmptyParts))
        insertRecipientIfValid(additionalRecipient);

    return recipients;
}

QVariantMap SendEmailAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName,
        emailAddresses(context, /*activeOnly*/ false, /*displayOnly*/ true).values().join(' '));

    return result;
}

} // namespace nx::vms::rules
