// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SoftTriggerData, (json), SoftTriggerData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SoftTriggerInfo, (json), SoftTriggerInfo_Fields)

} // namespace nx::vms::rules
