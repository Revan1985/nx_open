// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_ioport_data.h>
#include <nx/utils/impl_ptr.h>

#include "base_camera_button_controller.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ExtendedOutputCameraButtonController:
    public BaseCameraButtonController
{
    Q_OBJECT
    using base_type = BaseCameraButtonController;

public:
    ExtendedOutputCameraButtonController(
        nx::vms::api::ExtendedCameraOutputs allowedOutputs,
        CameraButtonData::Group buttonGroup,
        QObject* parent = nullptr);
    virtual ~ExtendedOutputCameraButtonController() override;

protected:
    virtual void setResourceInternal(const QnResourcePtr& resource) override;

private:
    virtual bool setButtonActionState(const CameraButtonData& button, ActionState state) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
