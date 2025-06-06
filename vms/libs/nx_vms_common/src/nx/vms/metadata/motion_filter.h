// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QRegion>

#include <nx/media/motion_detection.h>

#include "metadata_archive.h"

namespace nx::vms::metadata {

struct NX_VMS_COMMON_API MotionFilter: public Filter
{
    QRegion region;
};

static const int kGridDataSizeBytes = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8;

class NX_VMS_COMMON_API MotionRecordMatcher: public nx::vms::metadata::RecordMatcher
{
    using base_type = RecordMatcher;
public:
    MotionRecordMatcher(const MotionFilter* filter);

    virtual bool matchRecord(int64_t timestampMs, const uint8_t* data, int recordSize) const override;
    const MotionFilter* filter() const;

    virtual bool isWholeFrame() const override { return m_wholeFrame; }
    virtual bool isEmpty() const override { return m_wholeFrame; }

private:
    int m_maskStart = 0;
    int m_maskEnd = 0;
    bool m_wholeFrame = false;

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) //< 'structure was padded due to alignment specifier'
#endif

    simd128i m_mask[kGridDataSizeBytes / sizeof(simd128i)];

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
};

} // namespace nx::vms::metadata
