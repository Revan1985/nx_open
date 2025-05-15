// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/camera_resource.h>

namespace nx::vms::client::desktop {

/**
 * It is a mock for real desktop camera which server should create at some moment, after
 * correspondent DesktopCameraPreloaderResource instance is created on client (and saved to server).
 * It is initially marked with Qn:fake flag.
 * When the server creates desktop camera, it updates correspondent DesktopCameraPreloaderResource,
 * and it loses its Qn::fake flag.
 */
class DesktopCameraPreloaderResource: public core::CameraResource
{
    Q_OBJECT
    using base_type = core::CameraResource;

public:
    DesktopCameraPreloaderResource(const nx::Uuid& id, const QString& physicalId);
};

using DesktopCameraPreloaderResourcePtr = QnSharedResourcePointer<DesktopCameraPreloaderResource>;
using DesktopCameraPreloaderResourceList =
    QnSharedResourcePointerList<DesktopCameraPreloaderResource>;

} // namespace nx::vms::client::desktop
