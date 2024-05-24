// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QValidator>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include "../field_validator.h"

namespace nx::vms::rules::utils {

template <class ValidationPolicy>
QValidator::State serversValidity(const QnMediaServerResourceList& servers)
{
    bool hasValid{false};
    bool hasInvalid{false};

    for (const auto& server: servers)
    {
        if (ValidationPolicy::isServerValid(server))
            hasValid = true;
        else
            hasInvalid = true;

        if (hasValid && hasInvalid)
            break;
    }

    if (!hasInvalid)
        return QValidator::State::Acceptable;

    return hasValid ? QValidator::State::Intermediate : QValidator::State::Invalid;
}

template <class ValidationPolicy>
ValidationResult camerasValidity(
    common::SystemContext* context,
    const QnVirtualCameraResourceList& cameras)
{
    bool hasValid{false};
    bool hasInvalid{false};

    for (const auto& camera: cameras)
    {
        if (ValidationPolicy::isResourceValid(camera))
            hasValid = true;
        else
            hasInvalid = true;

        if (hasValid && hasInvalid)
            break;
    }

    if (!hasInvalid)
        return {};

    return {
        hasValid ? QValidator::State::Intermediate : QValidator::State::Invalid,
        ValidationPolicy::getText(context, cameras)};
}

template <class ValidationPolicy>
ValidationResult cameraValidity(common::SystemContext* context, QnVirtualCameraResourcePtr device)
{
    if (ValidationPolicy::isResourceValid(device))
        return {};

    return {
        QValidator::State::Invalid,
        ValidationPolicy::getText(context, {device})};
}

} // namespace nx::vms::rules::utils