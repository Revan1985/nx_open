// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>

namespace nx::vms::client::desktop {

class VideoWallShortcutHelper
{
public:
    bool shortcutExists(const QnVideoWallResourcePtr& videowall) const;
    bool createShortcut(const QnVideoWallResourcePtr& videowall) const;
    bool deleteShortcut(const QnVideoWallResourcePtr& videowall) const;

    static void setVideoWallAutorunEnabled(
        const nx::Uuid& videowallUuid,
        bool value,
        const nx::Url& serverUrl = {});

    static bool canStartVideoWall(const QnVideoWallResourcePtr& videowall);
};

} // namespace nx::vms::client::desktop
