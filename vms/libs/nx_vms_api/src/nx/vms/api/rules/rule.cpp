// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rule, (json)(ubjson),
    nx_vms_api_rules_Rule_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(RuleV4, (json)(ubjson),
    nx_vms_api_rules_RuleV4_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResetRules, (json)(ubjson),
    nx_vms_api_rules_ResetRules_Fields, (brief, true))

} // namespace nx::vms::api::rules
