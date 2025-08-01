// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CAM_DISPLAY_H
#define QN_CAM_DISPLAY_H

#include <chrono>

#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg/abstract_video_decoder.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <utils/common/adaptive_sleep.h>
#include <utils/media/externaltimesource.h>

#include "video_stream_display.h"

// TODO: #sivanov use forward declaration.
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <nx/media/audio/format.h>
#include <nx/media/stream_event.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/lockable.h>
#include <nx/vms/client/desktop/camera/abstract_video_display.h>
#include <nx/vms/client/desktop/camera/audio_decode_mode.h>

class QnResourceWidgetRenderer;
class QnVideoStreamDisplay;
class QnAudioStreamDisplay;
class QnCompressedVideoData;
class QnAbstractArchiveStreamReader;

namespace nx::vms::client::core { struct SpectrumData; }

/**
  * Stores QnVideoStreamDisplay for each channel/sensor
  */
class QnCamDisplay:
    public QnAbstractDataConsumer,
    public QnlTimeSource,
    public nx::vms::client::desktop::AbstractVideoDisplay
{
    using base_type = QnAbstractDataConsumer;
    Q_OBJECT
public:
    /** Owns and stops reader in display thread. */
    QnCamDisplay(QnMediaResourcePtr resource, QnAbstractArchiveStreamReader* reader);
    ~QnCamDisplay();

    void addVideoRenderer(int channelCount, QnResourceWidgetRenderer* vw, bool canDownscale);
    void removeVideoRenderer(QnResourceWidgetRenderer* vw);
    int channelsCount() const;

    void addMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);
    void removeMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);

    virtual bool processData(const QnAbstractDataPacketPtr& data) override ;

    virtual void pleaseStop() override;

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    bool doDelayForAudio(QnConstCompressedAudioDataPtr ad, float speed);
    bool isAudioBuffering() const;
    void playAudio(bool play);
    virtual float getSpeed() const override; //< From AbstractVideoDisplay

    // schedule to clean up buffers all;
    // schedule - coz I do not want to introduce mutexes
    //I assume that first incoming frame after jump is keyframe

    /**
     * \returns                         Current time in microseconds.
     */

    virtual qint64 getCurrentTime() const override;
    virtual qint64 getDisplayedTime() const override;
    virtual qint64 getExternalTime() const override;
    virtual qint64 getNextTime() const override;

    virtual void endOfRun() override;

    void setMTDecoding(bool value);

    QSize getFrameSize(int channel) const;
    QImage getScreenshot(const QnLegacyTranscodingSettings& imageProcessingParams, bool anyQuality);
    QImage getGrayscaleScreenshot(int channel);
    virtual QSize getVideoSize() const override; //< From AbstractVideoDisplay
    virtual bool isRealTimeSource() const override; //< From AbstractVideoDisplay

    void setExternalTimeSource(QnlTimeSource* value);

    virtual bool canAcceptData() const override;
    bool isLongWaiting() const;
    bool isEOFReached() const;
    bool isStillImage() const;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual QSize getMaxScreenSize() const override; //< From AbstractVideoDisplay
    QnAbstractArchiveStreamReader* getArchiveReader() const;
    virtual bool isFullScreen() const override; //< From AbstractVideoDisplay
    virtual bool isZoomWindow() const override; //< From AbstractVideoDisplay
    void setFullScreen(bool fullScreen);

    virtual bool isFisheyeEnabled() const override; //< From AbstractVideoDisplay
    void setFisheyeEnabled(bool fisheyeEnabled);

    virtual bool isBuffering() const override; //< From AbstractVideoDisplay & QnlTimeSource

    QnAspectRatio overridenAspectRatio() const;
    void setOverridenAspectRatio(QnAspectRatio aspectRatio);

    const QSize& getRawDataSize() const {
        return m_display[0]->getRawDataSize();
    }

    QnMediaResourcePtr resource() const;

    nx::media::StreamEventPacket lastMediaEvent() const;

    virtual QString getName() const override;
    virtual bool isRadassSupported() const override;

    virtual CameraID getCameraID() const override;
    // Forwarded to reader
    virtual MediaQuality getQuality() const override;
    virtual void setQuality(MediaQuality quality, bool fastSwitch) override;
    virtual bool isPaused() const override;
    virtual bool isMediaPaused() const override;

    // Forwarded to QnAbstractDataConsumer
    virtual int dataQueueSize() const override;
    virtual int maxDataQueueSize() const override;

    virtual void setCallbackForStreamChanges(std::function<void()> callback) override;

    nx::vms::client::desktop::AudioDecodeMode audioDecodeMode() const;
    void setAudioDecodeMode(nx::vms::client::desktop::AudioDecodeMode decodeMode);
    nx::vms::client::core::SpectrumData audioSpectrum() const;
    QString codecName();

public slots:
    void onBeforeJump(qint64 time);
    void onSkippingFrames(qint64 time);
    void onJumpOccurred(qint64 time);
    void onRealTimeStreamHint(bool value);
    void onReaderPaused();
    void onReaderResumed();
    void onPrevFrameOccurred();
    void onNextFrameOccurred();

signals:
    void liveMode(bool value);
    void stillImageChanged();

protected:
    virtual void setSingleShotMode(bool single) override;
    virtual void setSpeed(float speed) override;

    bool haveAudio(float speed) const;

    // puts in queue and returns first in queue
    QnCompressedVideoDataPtr nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel);

    // this function doest not change any queues; it just returns time of next frame been displayed
    quint64 nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const;

    quint64 nextVideoImageTime(int channel) const;

    void clearVideoQueue();
    void enqueueVideo(QnCompressedVideoDataPtr vd);
    QnCompressedVideoDataPtr dequeueVideo(int channel);
    bool isAudioHoleDetected(QnCompressedVideoDataPtr vd);
    void afterJump(QnAbstractMediaDataPtr media);
    void processNewSpeed(float speed);
    bool useSync(QnConstAbstractMediaDataPtr md);
    int getBufferingMask();

    void at_finished() override;

private:
    /** Process incoming video frame and clear the video queue if possible. */
    bool processVideoData(QnCompressedVideoDataPtr incoming, int channel, float speed);

    /** Push video frame to decode and display the video frame and create the appropriate delay.*/
    bool display(QnCompressedVideoDataPtr vd, bool sleep, float speed);

    void hurryUpCheck(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForCamera(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCheckForLocalFile(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime);
    void hurryUpCkeckForCamera2(QnAbstractMediaDataPtr media);
    qint64 getMinReverseTime() const;

    qint64 getDisplayedMax() const;
    qint64 getDisplayedMin() const;
    void setAudioBufferSize(int bufferSize, int prebufferMs);

    void blockTimeValue(qint64 time);
    void blockTimeValueSafe(qint64 time); // can be called from other thread
    void unblockTimeValue();
    void waitForFramesDisplayed();
    void restoreVideoQueue(QnCompressedVideoDataPtr incoming, QnCompressedVideoDataPtr vd, int channel);
    template <class T> void markIgnoreBefore(const T& queue, qint64 time);
    bool needBuffering(qint64 vTime) const;
    void processSkippingFramesTime();
    qint64 doSmartSleep(const qint64 needToSleep, float speed);

    int maxDataQueueSize(bool live) const;

    static qint64 initialLiveBufferUsec();
    static qint64 maximumLiveBufferUsec();

    void moveTimestampTo(qint64 timestampUs);
    void processFillerPacket(
        qint64 timestampUs,
        QnAbstractStreamDataProvider* dataProvider,
        QnAbstractMediaData::MediaFlags flags);
    bool useRealTimeHurryUp() const;
    void processMetadata(const QnAbstractCompressedMetadataPtr& metadata);
    void notifyExternalTimeSrcAboutEof(bool isEof);
    bool isNvrFillerPacket(qint64 timestampUs) const;
    void pushMetadataToConsumers(const QnAbstractCompressedMetadataPtr& metadata);
protected:

    struct AudioFormat
    {
        bool operator==(const AudioFormat&) const = default;

        nx::media::audio::Format format;
        int bitsPerCodedSample = -1;
    };

    QnVideoStreamDisplay* m_display[CL_MAX_CHANNELS];
    QQueue<QnCompressedVideoDataPtr> m_videoQueue[CL_MAX_CHANNELS];

    QnAudioStreamDisplay* m_audioDisplay;

    QnAdaptiveSleep m_delay;

    float m_speed;
    float m_prevSpeed;

    bool m_playAudio;
    bool m_shouldPlayAudio = false;
    bool m_needChangePriority;
    bool m_hadAudio; // got at least one audio packet

    qint64 m_lastAudioPacketTime;
    qint64 m_syncAudioTime;
    int m_totalFrames;
    int m_iFrames;
    qint64 m_lastVideoPacketTime;
    qint64 m_lastDecodedTime;
    qint64 m_previousVideoTime;
    quint64 m_lastNonZerroDuration;
    qint64 m_lastSleepInterval;
    bool m_afterJump;
    bool m_bofReceived;
    bool m_ignoringVideo;
    bool m_isRealTimeSource;
    nx::media::audio::Format m_expectedAudioFormat;
    mutable nx::Mutex m_audioChangeMutex;
    bool m_videoBufferOverflow;
    bool m_singleShotMode;
    bool m_singleShotQuantProcessed;
    qint64 m_jumpTime;
    AudioFormat m_playingFormat;
    QnAbstractVideoDecoder::DecodeMode m_lightCpuMode;
    QnVideoStreamDisplay::FrameDisplayStatus m_lastFrameDisplayed;
    bool m_realTimeHurryUp;
    int m_delayedFrameCount;
    QnlTimeSource* m_extTimeSrc;

    bool m_useMtDecoding;
    int m_buffering;
    int m_processedPackets;
    qint64 m_nextReverseTime[CL_MAX_CHANNELS];
    int m_emptyPacketCounter;
    bool m_isStillImage;
    bool m_isLongWaiting;
    qint64 m_skippingFramesTime;

    bool m_executingChangeSpeed;
    bool m_eofSignalSent;
    int m_audioBufferSize;
    qint64 m_minAudioDetectJumpInterval;
    qint64 m_videoQueueDuration;
    bool m_useMTRealTimeDecode; // multi thread decode for live temporary allowed
    bool m_forceMtDecoding; // force multi thread decode in any case

    mutable nx::Mutex m_timeMutex;
    QnMediaResourcePtr m_resource;
    QElapsedTimer m_afterJumpTimer;
    qint64 m_firstAfterJumpTime;
    qint64 m_receivedInterval;
    std::unique_ptr<QnAbstractArchiveStreamReader> m_archiveReader;

    bool m_fullScreen;
    int m_prevLQ;
    bool m_doNotChangeDisplayTime;
    bool m_multiView;
    bool m_fisheyeEnabled;
    int m_channelsCount;

    qint64 m_lastQueuedVideoTime;
    int m_liveBufferSizeUsec;
    bool m_liveMaxLenReached;
    bool m_hasVideo;
    nx::media::StreamEventPacket m_lastMediaEvent;
    mutable nx::Mutex m_lastMediaEventMutex;
    nx::utils::ElapsedTimer m_lastMediaEventTimeout;

    mutable nx::Mutex m_metadataConsumersHashMutex;
    QMultiMap<MetadataType, QWeakPointer<nx::media::AbstractMetadataConsumer>>
        m_metadataConsumerByType;
    QVector<bool> m_gotKeyDataInfo;
    std::function<void()> m_streamsChangedCallback;

    nx::vms::client::desktop::AudioDecodeMode m_audioDecodeMode =
        nx::vms::client::desktop::AudioDecodeMode::normal;
    nx::Lockable<QString> m_codecName;
    AVCodecID m_codecId = AV_CODEC_ID_NONE;
};

#endif //QN_CAM_DISPLAY_H
