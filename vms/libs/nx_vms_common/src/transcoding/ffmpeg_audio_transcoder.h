// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <stdint.h>

#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/byte_array.h>
#include <transcoding/abstract_codec_transcoder.h>
#include <transcoding/ffmpeg_audio_resampler.h>


//< Minimal supported sample rate in mp4(with mp3), see ffmpeg(7.0.1) movenc.c:7579
constexpr int kMinMp4Mp3SampleRate = 16000;

/**
 * Transcodes audio packets from one format to another.
 * Can be used to change codec and/or sample rate.
 */
class NX_VMS_COMMON_API QnFfmpegAudioTranscoder: public AbstractCodecTranscoder
{
public:
    struct Config
    {
        AVCodecID targetCodecId = AV_CODEC_ID_NONE;
        int bitrate = -1;
        // Sets destination frame size.
        int dstFrameSize = 0;
    };

public:
    /**
     * \param codecId Id of destination codec.
     */
    QnFfmpegAudioTranscoder(const Config& config);
    ~QnFfmpegAudioTranscoder();

    /**
     * \brief Transcodes packet from one codec/sample rate to another ones.
     * \param[in] media Encoded audio data that should be transcoded.
     * \param[out] result Pointer to media packet transcoded data will be written in.
     * Can be equal to nullptr after function returns if there is not enough data to create output
     * packet.
     * \return 0 on success, error code otherwise.
     */
    virtual int transcodePacket(
        const QnConstAbstractMediaDataPtr& media,
        QnAbstractMediaDataPtr* const result) override;

    /**
     * \brief Sets up decoder context and opens it.
     * \param[in] audio Encoded audio packet. It's required that proper decoding context was set for
     * that packet.
     * \return true on success, false in case of error
     */
    bool open(const QnConstCompressedAudioDataPtr& audio);

    /**
     * \brief Sets up decoder context and opens it.
     * \param context Decoder context.
     * \return true on success, false in case of error
     */
    bool open(const CodecParametersConstPtr& context);

    void close();

    /**
     * \return true if last call for open returns true, false otherwise.
     */
    bool isOpened() const;

    /**
     * \return encoder context.
     */
    AVCodecContext* getCodecContext();

    AVCodecParameters* getCodecParameters();

    /**
     * \brief Sets destination sample rate.
     * \param value Desired destination sample rate.
     */
    void setSampleRate(int value);
    bool sendPacket(const QnConstAbstractMediaDataPtr& media);
    bool receivePacket(QnAbstractMediaDataPtr* const result);

    //!Get output bitrate bitrate (bps)
    int getBitrate() const { return m_bitrate; }

private:
    void tuneContextsWithMedia(
        AVCodecContext* inCtx,
        AVCodecContext* outCtx,
        const QnConstAbstractMediaDataPtr& media);

    std::size_t getSampleMultiplyCoefficient(const AVCodecContext* ctx);
    std::size_t getPlanesCount(const AVCodecContext* ctx);
    bool initResampler();

    QnAbstractMediaDataPtr createMediaDataFromAVPacket(const AVPacket& packet);

private:
    const Config m_config;
    AVCodecContext* m_encoderCtx = nullptr;
    AVCodecContext* m_decoderCtx = nullptr;
    std::unique_ptr<FfmpegAudioResampler> m_resampler;

    /**
     * Wrapper for m_encoderContext.
     * Used to create our own media packets from AVPacket ffmpeg structure.
     */
    CodecParametersConstPtr m_codecParamaters;

    int m_dstSampleRate = 0;
    bool m_isOpened = false;
    int m_channelNumber = 0;
    int m_bitrate = -1;
};

using QnFfmpegAudioTranscoderPtr = std::unique_ptr<QnFfmpegAudioTranscoder>;
