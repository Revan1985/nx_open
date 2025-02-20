// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_device_field.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/openapi_doc.h>

#include "../manifest.h"
#include "../utils/resource.h"

namespace nx::vms::rules {

nx::Uuid TargetDeviceField::id() const
{
    return m_id;
}

void TargetDeviceField::setId(nx::Uuid id)
{
    if (m_id == id)
        return;

    m_id = id;
    emit idChanged();
}

bool TargetDeviceField::useSource() const
{
    return m_useSource;
}

void TargetDeviceField::setUseSource(bool value)
{
    if (m_useSource == value)
        return;

    m_useSource = value;
    emit useSourceChanged();
}

QVariant TargetDeviceField::build(const AggregatedEventPtr& event) const
{
    if (m_useSource)
    {
        const auto deviceIds = utils::getDeviceIds(event);

        // Use single camera (the last one). Synced with the old engine.
        if (NX_ASSERT(!deviceIds.isEmpty()))
            return QVariant::fromValue(deviceIds.last());

        return {};
    }

    return QVariant::fromValue(m_id);
}

TargetSingleDeviceFieldProperties TargetDeviceField::properties() const
{
    return TargetSingleDeviceFieldProperties::fromVariantMap(descriptor()->properties);
}

QJsonObject TargetDeviceField::openApiDescriptor(const QVariantMap&)
{
    auto descriptor = utils::constructOpenApiDescriptor<TargetDeviceField>();
    descriptor[utils::kDescriptionProperty] =
        "Specifies the target resource for the action. "
        "If the <code>useSource</code> flag is set, the <code>id</code> will be ignored";
    utils::updatePropertyForField(descriptor,
        "useSource",
        utils::kDescriptionProperty,
        "Controls whether the device ID from the event itself should be used "
        "as target resource for action.");
    return descriptor;
}

} // namespace nx::vms::rules
