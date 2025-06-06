// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API ServerAndDeviceIdData
{
    nx::Uuid deviceId;
    nx::Uuid serverId;

    bool operator==(const ServerAndDeviceIdData& other) const = default;
};
#define ServerAndDeviceIdData_Fields \
    (deviceId) \
    (serverId)

QN_FUSION_DECLARE_FUNCTIONS(ServerAndDeviceIdData, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ServerAndDeviceIdData, ServerAndDeviceIdData_Fields)

} // namespace nx::vms::api
