// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>

#include "bookmark_models.h"
#include "media_settings.h"

namespace nx::vms::api {

struct NX_VMS_API StreamSettings: MediaSettings
{
    static const QString kMpjpegBoundary;
    static constexpr int kDefaultMaxCachedFrames = 1;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(ValidationResult,
        invalidFormat,
        isValid
    );

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Format,
        webm,
        mpegts,
        mpjpeg,
        mp4,
        mkv,
        _3gp,
        rtp,
        flv,
        f4v
    );

    /**%apidoc[opt] Stream format. */
    Format format = Format::mkv;

    /**%apidoc[opt] Video quality. */
    StreamQuality quality = StreamQuality::normal;

    /**%apidoc[opt]
     * If present, specifies the Archive stream end position. It is used only if the `positionMs`
     * parameter is present.
     */
    std::optional<std::chrono::milliseconds> endPositionMs = std::nullopt;

    /**%apidoc[opt] Drop Late Frames. */
    std::optional<int> dropLateFrames = std::nullopt;

    /**%apidoc[opt]
     * Stand Frame Duration. If the parameter is present, the video speed is limited by the real
     * time.
     */
    bool standFrameDuration = false;

    /**%apidoc[opt]
     * Turn on the realtime optimization. It will drop some frames if there is not enough CPU for
     * the realtime transcoding.
     */
    bool realTimeOptimization = false;

    /**%apidoc[opt] Send only the audio stream. */
    bool audioOnly = false;

    /**%apidoc[opt]
     * Seek to the exact time in the Archive by the specified `pos`, otherwise seek to the nearest
     * left to the `pos` keyframe (on timeline). Enabling causes the stream to be transcoded.
     * Disabled by default.
     */
    bool accurateSeek = false;

    /**%apidoc[opt] Can be used for both live and archive streams -
     * for archive streams effectively it's another way to specify `endPositionMs`.
     */
    std::optional<std::chrono::milliseconds> durationMs = std::nullopt;

    /**%apidoc[opt] Add signature to exported media data, only mp4 and webm
     * formats are supported.
     */
    bool signature = false;

    /**%apidoc[opt] Use absolute UTC timestamps in exported media data,
     * only mp4 format is supported.
     */
    bool utcTimestamps = false;

    /**%apidoc[opt]
     * %deprecated Add continuous timestamps in exported media data. Always true, regardless of
     * the parameter value.
     */
    bool continuousTimestamps = true;

    /**%apidoc[opt] Force to download file in browser instead of displaying it.
     */
    bool download = false;

    ValidationResult validateStreamSettings() const;

    static QByteArray getMimeType(const QString& format);
};
#define StreamSettings_Fields MediaSettings_Fields(format)(quality)(endPositionMs)\
    (dropLateFrames)(standFrameDuration)(realTimeOptimization)(audioOnly)(accurateSeek)\
    (durationMs)(signature)(utcTimestamps)(continuousTimestamps)(download)
QN_FUSION_DECLARE_FUNCTIONS(StreamSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StreamSettings, StreamSettings_Fields)


struct NX_VMS_API MediaStreamSettings: public StreamSettings
{
    /**%apidoc
     * Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * /rest/v{1-}/devices) or MAC address (not supported for certain Devices).
     */
    nx::Uuid id;
};
#define MediaStreamSettings_Fields StreamSettings_Fields(id)
QN_FUSION_DECLARE_FUNCTIONS(MediaStreamSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MediaStreamSettings, MediaStreamSettings_Fields)

/**%apidoc
 * %param[opt] durationMs Fragment length in milliseconds.
 * %param[unused] download
 */
struct NX_VMS_API BookmarkStreamSettings: public StreamSettings, public BookmarkProtection
{
    /** Can be obtained from `/rest/v{4-}/devices/&ast;/bookmarks`. */
    QString bookmarkId;
};
#define BookmarkStreamSettings_Fields StreamSettings_Fields BookmarkProtection_Fields (bookmarkId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkStreamSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BookmarkStreamSettings, BookmarkStreamSettings_Fields)

} // namespace nx::vms::api
