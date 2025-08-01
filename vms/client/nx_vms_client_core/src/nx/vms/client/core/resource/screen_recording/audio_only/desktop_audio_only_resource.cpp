// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_audio_only_resource.h"

#include "desktop_audio_only_data_provider.h"

namespace nx::vms::client::core {

bool DesktopAudioOnlyResource::isRendererSlow() const
{
    return false;
}

bool DesktopAudioOnlyResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return false;
}

AudioLayoutConstPtr DesktopAudioOnlyResource::getAudioLayout(
    const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return nullptr;
}

QnAbstractStreamDataProvider* DesktopAudioOnlyResource::createDataProvider(
    Qn::ConnectionRole /*role*/)
{
    return new DesktopAudioOnlyDataProvider();
}

} // namespace nx::vms::client::core
