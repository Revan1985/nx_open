// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_server_field_validator.h"

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/server_validation_policy.h>

#include "../action_builder_fields/target_servers_field.h"
#include "../manifest.h"
#include "../strings.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetServerFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto targetServerField = dynamic_cast<const TargetServersField*>(field);
    if (!NX_ASSERT(targetServerField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto serversSelection = targetServerField->selection();
    const auto targetServerFieldProperties = targetServerField->properties();
    const bool isValidSelection =
        !serversSelection.ids.empty() || targetServerFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectServer()};

    QnMediaServerResourceList servers = utils::servers(serversSelection, context);
    if (servers.empty() && !serversSelection.ids.empty())
        return {QValidator::State::Invalid, Strings::serversWereRemoved(serversSelection.ids.size())};

    if (!targetServerFieldProperties.validationPolicy.isEmpty())
    {
        if (targetServerFieldProperties.validationPolicy == kHasBuzzerValidationPolicy)
        {
            const auto serversValidity = utils::serversValidity<QnBuzzerPolicy>(servers);

            if (serversValidity == QValidator::State::Acceptable)
                return {};

            return {serversValidity, Strings::noSuitableServers(serversValidity)};
        }

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    return {};
}

} // namespace nx::vms::rules
