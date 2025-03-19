// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_camera_field_validator.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/camera_validation_policy.h>

#include "../event_filter_fields/source_camera_field.h"
#include "../manifest.h"
#include "../strings.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult SourceCameraFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto sourceCameraField = dynamic_cast<const SourceCameraField*>(field);
    if (!NX_ASSERT(sourceCameraField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto sourceCameraFieldProperties = sourceCameraField->properties();
    const auto cameraIds = sourceCameraField->ids();
    const bool isValidSelection =
        !cameraIds.empty() || sourceCameraFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectCamera(context)};

    const auto cameras =
        context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(cameraIds);
    if (cameras.empty() && !cameraIds.empty())
        return {QValidator::State::Invalid, Strings::camerasWereRemoved(context, cameraIds.size())};

    if (!sourceCameraFieldProperties.validationPolicy.isEmpty())
    {
        ValidationResult validationResult;
        if (sourceCameraFieldProperties.validationPolicy == kCameraAnalyticsEventsValidationPolicy)
            return utils::camerasValidity<QnCameraAnalyticsEventsPolicy>(context, cameras);

        if (sourceCameraFieldProperties.validationPolicy == kCameraAnalyticsObjectsValidationPolicy)
            return utils::camerasValidity<QnCameraAnalyticsObjectsPolicy>(context, cameras);

        if (sourceCameraFieldProperties.validationPolicy == kCameraInputValidationPolicy)
            return utils::camerasValidity<QnCameraInputPolicy>(context, cameras);

        if (sourceCameraFieldProperties.validationPolicy == kCameraMotionValidationPolicy)
            return utils::camerasValidity<QnCameraMotionPolicy>(context, cameras);

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    return {};
}

} // namespace nx::vms::rules
