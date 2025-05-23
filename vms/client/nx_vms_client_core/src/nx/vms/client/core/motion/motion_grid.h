// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <climits>

#include <nx/media/config.h>
#include <nx/media/motion_detection.h>

class QPoint;
class QRect;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API MotionGrid
{
public:
    static constexpr int kWidth = Qn::kMotionGridWidth;
    static constexpr int kHeight = Qn::kMotionGridHeight;
    static constexpr int kCellCount = kWidth * kHeight;

    /** Bitmask representation of the motion grid. */
    static constexpr int kGridByteSize = kCellCount / CHAR_BIT;
    static_assert(kGridByteSize % CHAR_BIT == 0);

public:
    // Construction & assignment.
    MotionGrid() = default;
    MotionGrid(const MotionGrid& other) = default;
    MotionGrid& operator=(const MotionGrid& other) = default;

    // Item access.
    int operator[](const QPoint& pos) const;
    int& operator[](const QPoint& pos);

    // Fill grid with zeros.
    void reset();

    // Fill specified rectangle with specified value.
    void fillRect(const QRect& rect, int value);

    // Fill consecutive region containing specified point with specified value.
    void fillRegion(const QPoint& at, int value);

    // Fill consecutive region containing specified point with zeros.
    void clearRegion(const QPoint& at);

    // Equality comparison.
    bool operator==(const MotionGrid& other) const;
    bool operator!=(const MotionGrid& other) const;

private:
    using Row = std::array<int, kWidth>;
    using Grid = std::array<Row, kHeight>;
    Grid m_grid = Grid();
};

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4324) //< 'structure was padded due to alignment specifier'
#endif

struct alignas(CL_MEDIA_ALIGNMENT) MotionGridBitMask:
    std::array<std::byte, MotionGrid::kGridByteSize>
{
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

} // namespace nx::vms::client::core
