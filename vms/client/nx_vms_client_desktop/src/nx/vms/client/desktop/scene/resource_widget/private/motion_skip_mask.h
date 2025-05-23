// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtGui/QRegion>

#include <core/resource/motion_window.h>
#include <nx/media/motion_detection.h>
#include <nx/vms/client/core/motion/motion_grid.h>

namespace nx::vms::client::desktop {

/**
 * Information about region where motion must not be detected.
 */
class MotionSkipMask
{
public:
    MotionSkipMask(QnRegion region);

    QRegion region() const;
    const char* const bitMask() const;

private:
    QnRegion m_region;

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) //< 'structure was padded due to alignment specifier'
#endif

    nx::vms::client::core::MotionGridBitMask m_bitMask;

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

};

} // namespace nx::vms::client::desktop
