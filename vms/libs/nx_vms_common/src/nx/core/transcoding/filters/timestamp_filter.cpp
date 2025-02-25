// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamp_filter.h"

#include <QtCore/QDateTime>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <nx/core/transcoding/filters/image_to_frame_painter.h>
#include <nx/core/transcoding/filters/transcoding_settings.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/util.h>

namespace {

QFont fontFromParams(const nx::core::transcoding::TimestampOverlaySettings& params)
{
    QFont result;
    result.setPixelSize(params.fontSize);
    result.setBold(true);
    return result;
}

} // namespace

namespace nx {
namespace core {
namespace transcoding {

class TimestampFilter::Internal
{
public:
    explicit Internal(const core::transcoding::TimestampOverlaySettings& params);

    void updateTimestamp(const CLVideoDecoderOutputPtr& frame);
    detail::ImageToFramePainter& painter();

private:
    const QFont m_font;
    const QFontMetrics m_fontMetrics;

    core::transcoding::TimestampOverlaySettings m_params;
    detail::ImageToFramePainter m_painter;
    qint64 m_currentTimeMs = -1;
};

TimestampFilter::Internal::Internal(const core::transcoding::TimestampOverlaySettings& params):
    m_font(fontFromParams(params)),
    m_fontMetrics(m_font),
    m_params(params)
{
}

void TimestampFilter::Internal::updateTimestamp(const CLVideoDecoderOutputPtr& frame)
{
    const qint64 displayTime = frame->pts / 1000;
    if (m_currentTimeMs == displayTime)
        return;

    m_currentTimeMs = displayTime;

    const auto timeString = displayTime * 1000 >= UTC_TIME_DETECTION_THRESHOLD
        ? timestampTextUtc(m_currentTimeMs, m_params.timeZone, m_params.format)
        : timestampTextSimple(m_currentTimeMs);

    const QSize textMargins(m_fontMetrics.averageCharWidth() / 2, 1);
    const QSize textSize = m_fontMetrics.size(0, timeString) + textMargins * 2;

    QImage image(textSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing);

    QPainterPath path;
    path.addText(textMargins.width(), m_fontMetrics.ascent(), m_font, timeString);
    painter.strokePath(path, QPen(m_params.outline, 2.0));
    painter.fillPath(path, m_params.foreground);

    m_painter.setImage(image, m_params.offset, m_params.alignment);
}

detail::ImageToFramePainter& TimestampFilter::Internal::painter()
{
    return m_painter;
}

///

TimestampFilter::TimestampFilter(const core::transcoding::TimestampOverlaySettings& params):
    d(new Internal(params))
{
}

TimestampFilter::~TimestampFilter()
{
}

CLVideoDecoderOutputPtr TimestampFilter::updateImage(
    const CLVideoDecoderOutputPtr& frame,
    const QnAbstractCompressedMetadataPtr&)
{
    d->updateTimestamp(frame);
    d->painter().drawTo(frame);
    return frame;
}

QSize TimestampFilter::updatedResolution(const QSize& sourceSize)
{
    return sourceSize;
}

QString TimestampFilter::timestampTextUtc(
    qint64 sinceEpochMs,
    const QTimeZone& timeZone,
    TimestampFormat format)
{
    using namespace nx::vms::time;
    // TODO: #sivanov nx::vms::time::toString should be expanded to accept different zones.
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(sinceEpochMs, timeZone);

    switch (format)
    {
        case TimestampFormat::longDate:
            return toString(dateTime, Format::dddd_d_MMMM_yyyy_hh_mm_ss);
        case TimestampFormat::shortDate:
            return toString(dateTime);
        case TimestampFormat::ISODate:
            return dateTime.toString(Qt::ISODate);
        case TimestampFormat::RFC2822Date:
            return dateTime.toString(Qt::RFC2822Date);
        default:
            NX_ASSERT(false, "Invalid DateTime Format");
            return QString();
    }
}

QString TimestampFilter::timestampTextSimple(qint64 timeOffsetMs)
{
    return nx::vms::time::toString(timeOffsetMs, nx::vms::time::Format::hh_mm_ss);
}

} // namespace transcoding
} // namespace core
} // namespace nx
