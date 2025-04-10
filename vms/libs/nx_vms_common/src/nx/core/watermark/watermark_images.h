// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

class QSize;

namespace nx::core {

struct Watermark;

// Creates image directly, ignores cache completely.
NX_VMS_COMMON_API QImage createWatermarkImage(const Watermark& watermark, const QSize& size);

// Retrieves image from cache. Creates and caches a new one if it does not exist.
// Drops the whole cache if watermark is different from previous.
// May return image with different size but the same aspect ratio.
NX_VMS_COMMON_API QImage retrieveWatermarkImage(const Watermark& watermark, const QSize& size);

} // namespace nx::core
