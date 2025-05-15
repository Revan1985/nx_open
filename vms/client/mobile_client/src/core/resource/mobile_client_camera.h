// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/camera_resource.h>

class QnMobileClientCamera: public nx::vms::client::core::CameraResource
{
    Q_OBJECT
    using base_type = nx::vms::client::core::CameraResource;

public:
    explicit QnMobileClientCamera(const nx::Uuid& resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) override;

    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

protected:
    virtual QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role) override;
};
