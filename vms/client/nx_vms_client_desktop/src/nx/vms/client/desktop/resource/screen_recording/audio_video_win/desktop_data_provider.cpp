// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_data_provider.h"

#include <windows.h>
//^ Windows header must be included first (actual for no-pch build only).

#include <intrin.h>
#include <mmsystem.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
} // extern "C"

#include <speex/speex_preprocess.h>

#include <core/resource/resource.h>
#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/av_options.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/old_api.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/media_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/resource/screen_recording/audio_device_info_win.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <transcoding/audio_encoder.h>
#include <utils/common/synctime.h>

#include "audio_device_change_notifier.h"
#include "buffered_screen_grabber.h"

namespace nx::vms::client::desktop {

namespace {

static const int AUDIO_QUEUE_MAX_SIZE = 256;
static const int AUDIO_CAPTURE_FREQUENCY = 44100;
static const int BASE_BITRATE = 1000 * 1000 * 10; // bitrate for best quality for fullHD mode;
static const int MAX_VIDEO_JITTER = 2;

} // namespace

class EncodedAudioInfo
{
public:
    static const int AUDIO_BUFFERS_COUNT = 2;
    EncodedAudioInfo(DesktopDataProvider* owner);
    ~EncodedAudioInfo();
    // doubled audio objects
    core::AudioDeviceInfo m_audioDevice;
    QAudioFormat m_audioFormat;
    QnSafeQueue<QnWritableCompressedAudioDataPtr>  m_audioQueue;
    QnWritableCompressedAudioData m_tmpAudioBuffer;
    SpeexPreprocessState* m_speexPreprocess = nullptr;

    int audioPacketSize();
    bool setupFormat(QString& errMessage);
    bool setupPostProcess(int frameSize, int channels, int sampleRate);
    qint64 currentTime() const { return m_owner->currentTime(); }
    bool start();
    void stop();
    int nameToWaveIndex();
    bool addBuffer();
    void gotAudioPacket(const char* data, int packetSize);
    void gotData();
    void clearBuffers();
private:
    DesktopDataProvider* const m_owner;
    HWAVEIN hWaveIn = 0;
    QQueue<WAVEHDR*> m_buffers;
    nx::Mutex m_mtx;
    std::atomic<bool> m_terminated = false;
    bool m_waveInOpened = false;
    std::vector<char> m_intermediateAudioBuffer;
};

EncodedAudioInfo::EncodedAudioInfo(DesktopDataProvider* owner):
    m_tmpAudioBuffer(AV_INPUT_BUFFER_MIN_SIZE),
    m_owner(owner)
{
}

EncodedAudioInfo::~EncodedAudioInfo()
{
    stop();
    if (m_speexPreprocess)
        speex_preprocess_state_destroy(m_speexPreprocess);
    m_speexPreprocess = 0;
}

int EncodedAudioInfo::nameToWaveIndex()
{
    int iNumDevs = waveInGetNumDevs();
    QString name = m_audioDevice.name();
    int devNum = m_audioDevice.deviceNumber();
    for (int i = 0; i < iNumDevs; ++i)
    {
        WAVEINCAPS wic;
        if(waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR)
        {
            // This may look like "Microphone (Realtek Hi".
            QString tmp = QString((const QChar *) wic.szPname);
            if (name.startsWith(tmp))
            {
                if (--devNum == 0)
                    return i;
            }
        }
    }
    return WAVE_MAPPER;
}

static void QT_WIN_CALLBACK waveInProc(HWAVEIN /*hWaveIn*/,
                                UINT uMsg,
                                DWORD_PTR dwInstance,
                                DWORD_PTR /*dwParam1*/,
                                DWORD_PTR /*dwParam2*/)
{
    EncodedAudioInfo* audio = (EncodedAudioInfo*) dwInstance;
    switch(uMsg)
    {
        case WIM_OPEN:
            break;
        case WIM_DATA:
            audio->gotData();
            break;
        case WIM_CLOSE:
            break;
        default:
            return;
    }
}

void EncodedAudioInfo::clearBuffers()
{
    while (m_buffers.size() > 0)
    {
        WAVEHDR* data = m_buffers.dequeue();
        waveInUnprepareHeader(hWaveIn, data, sizeof(WAVEHDR));
        av_free(data->lpData);
        delete data;
    }
}

void EncodedAudioInfo::gotAudioPacket(const char* data, int packetSize)
{
    QnWritableCompressedAudioDataPtr outData(new QnWritableCompressedAudioData(packetSize));
    outData->m_data.write(data, packetSize);
    outData->timestamp = m_owner->currentTime(); // - m_startDelay;
    m_audioQueue.push(outData);
}

void EncodedAudioInfo::gotData()
{
    NX_MUTEX_LOCKER lock( &m_mtx );
    if (m_terminated)
        return;

    if (m_buffers.isEmpty())
        return;
    WAVEHDR* data = m_buffers.front();
    if (data->dwBytesRecorded > 0 && data->dwFlags & WHDR_DONE)
    {
        // write data
        if (m_intermediateAudioBuffer.empty() && data->dwBytesRecorded == data->dwBufferLength)
        {
            gotAudioPacket(data->lpData, data->dwBufferLength);
        }
        else
        {
            std::copy(data->lpData, data->lpData + data->dwBytesRecorded,
                std::back_inserter(m_intermediateAudioBuffer));
            if (m_intermediateAudioBuffer.size() >= data->dwBufferLength)
            {
                gotAudioPacket(m_intermediateAudioBuffer.data(), data->dwBufferLength);
                m_intermediateAudioBuffer.erase(
                    m_intermediateAudioBuffer.begin(),
                    m_intermediateAudioBuffer.begin() + data->dwBufferLength);
            }
        }
        waveInUnprepareHeader(hWaveIn, data, sizeof(WAVEHDR));
        av_free(data->lpData);
        delete data;
        m_buffers.dequeue();
        addBuffer();
    }
}

bool EncodedAudioInfo::addBuffer()
{
    WAVEHDR* buffer = new WAVEHDR();
    HRESULT hr;
    memset(buffer, 0, sizeof(WAVEHDR));
    buffer->dwBufferLength = audioPacketSize();
    buffer->lpData = (LPSTR) av_malloc(audioPacketSize());

    m_buffers << buffer;

    hr = waveInPrepareHeader(hWaveIn, buffer, sizeof(WAVEHDR));
    if (hr != S_OK)
    {
        NX_WARNING(this, "waveInPrepareHeader call failed. Error: %1", hr);
        return false;
    }

    hr = waveInAddBuffer(hWaveIn, buffer, sizeof(WAVEHDR));
    if (hr != S_OK)
    {
        NX_WARNING(this, "waveInAddBuffer call failed. Error: %1", hr);
        return false;
    }

    return true;
}

void EncodedAudioInfo::stop()
{
    {
        NX_MUTEX_LOCKER lock( &m_mtx );
        m_terminated = true;
        if (!m_waveInOpened)
            return;
        m_waveInOpened = false;
    }
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    waveInClose(hWaveIn);
    clearBuffers();
}

bool EncodedAudioInfo::start()
{
    NX_MUTEX_LOCKER lock(&m_mtx);
    if (m_terminated)
        return false;
    return waveInStart(hWaveIn) == S_OK;
}

int EncodedAudioInfo::audioPacketSize()
{
    return m_owner->frameSize() * m_audioFormat.channelCount() * m_audioFormat.bytesPerSample();
}

bool EncodedAudioInfo::setupFormat(QString& errMessage)
{
    int deviceID = nameToWaveIndex();
    WAVEINCAPS deviceCaps;
    MMRESULT result = waveInGetDevCaps(deviceID, &deviceCaps, sizeof(WAVEINCAPS));
    if (result != MMSYSERR_NOERROR)
    {
        NX_WARNING(this, "Failed to get audio device capability, id: %1, error: %2",
            deviceID, result);
        return false;
    }
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);
    m_audioFormat.setChannelCount(deviceCaps.wChannels); //< Stereo or mono.

    // Check 16 bit formats only.
    if (deviceCaps.dwFormats & WAVE_FORMAT_4S16 || deviceCaps.dwFormats & WAVE_FORMAT_4M16)
    {
        m_audioFormat.setSampleRate(44100);
    }
    else if (deviceCaps.dwFormats & WAVE_FORMAT_2S16 || deviceCaps.dwFormats & WAVE_FORMAT_2M16)
    {
        m_audioFormat.setSampleRate(22050);
    }
    else if (deviceCaps.dwFormats & WAVE_FORMAT_1S16 || deviceCaps.dwFormats & WAVE_FORMAT_1M16)
    {
        m_audioFormat.setSampleRate(11025);
    }
    else
    {
        m_audioFormat = {};
        NX_DEBUG(this, "16 bit audio format not found: %1.", deviceCaps.dwFormats);
        return false;
    }

    m_audioQueue.setMaxSize(AUDIO_QUEUE_MAX_SIZE);
    NX_DEBUG(this, "Selected audio format for audio device: %1.", m_audioFormat);
    return true;
}

bool EncodedAudioInfo::setupPostProcess(
    int frameSize,
    int channels,
    int sampleRate)
{
    NX_MUTEX_LOCKER lock(&m_mtx);
    if (m_terminated)
        return false;

    int devId = nameToWaveIndex();
    WAVEFORMATEX wfx;
    //HRESULT hr;
    wfx.nSamplesPerSec = m_audioFormat.sampleRate();
    wfx.wBitsPerSample = m_audioFormat.bytesPerSample() * 8;
    wfx.nChannels = m_audioFormat.channelCount();
    wfx.cbSize = 0;

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    MMRESULT result = waveInOpen(&hWaveIn, devId, &wfx,
        (DWORD_PTR)&waveInProc,
        (DWORD_PTR)this,
        CALLBACK_FUNCTION) != MMSYSERR_NOERROR;
    if(result != S_OK)
    {
        NX_WARNING(this, "waveInOpen call failed. Error: %1", (int) result);
        return false;
    }
    m_waveInOpened = true;
    for (int i = 0; i < AUDIO_BUFFERS_COUNT; ++i)
    {
        if (!addBuffer())
            return false;
    }

    core::WinAudioDeviceInfo extInfo(m_audioDevice.description());
    if (extInfo.isMicrophone())
    {
        m_speexPreprocess = speex_preprocess_state_init(frameSize * channels, sampleRate);
        int denoiseEnabled = 1;
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoiseEnabled);

        int gainLevel = 16;
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC_INCREMENT, &gainLevel);
        speex_preprocess_ctl(m_speexPreprocess, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &gainLevel);
    }
    return true;
}

struct DesktopDataProvider::Private
{
    const int desktopNum;
    const screen_recording::CaptureMode captureMode;
    const bool captureCursor;
    const QSize captureResolution;
    const float encodeQualuty;
    const QString videoCodec;
    QWidget* const widget;
    const QPixmap logo;

    int encodedFrames = 0;
    BufferedScreenGrabber* grabber = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    CodecParametersConstPtr videoContext;
    AVFrame* frame = nullptr;

    QVector<quint8> buffer;

    // single audio objects
    int audioFramesCount = 0;
    double audioFrameDuration = 0.0;
    qint64 storedAudioPts = 0;
    int maxAudioJitter = 0;
    QVector<EncodedAudioInfo*> audioInfo;

    bool capturingStopped = false;

    qint64 initTime;
    mutable nx::Mutex startMutex;
    bool started = false;
    bool isAudioInitialized = false;
    bool isVideoInitialized = false;

    QPointer<nx::vms::client::core::VoiceSpectrumAnalyzer> soundAnalyzer;
    int frameSize = 0;
    std::unique_ptr<QElapsedTimer> timer;
    std::unique_ptr<AudioDeviceChangeNotifier> audioDeviceChangeNotifier =
        std::make_unique<AudioDeviceChangeNotifier>();
};

DesktopDataProvider::DesktopDataProvider(
    int desktopNum,
    const core::AudioDeviceInfo* audioDevice,
    const core::AudioDeviceInfo* audioDevice2,
    screen_recording::CaptureMode captureMode,
    bool captureCursor,
    const QSize& captureResolution,
    float encodeQualuty, //< In range [0.0, 1.0].
    const QString& videoCodec,
    QWidget* glWidget,
    const QPixmap& logo)
    :
    d(new Private{
        .desktopNum = desktopNum,
        .captureMode = captureMode,
        .captureCursor = captureCursor,
        .captureResolution = captureResolution,
        .encodeQualuty = encodeQualuty,
        .videoCodec = videoCodec,
        .widget = glWidget,
        .logo = logo
        })
{
    if (audioDevice || audioDevice2)
    {
        d->audioInfo << new EncodedAudioInfo(this);
        d->audioInfo[0]->m_audioDevice = audioDevice ? *audioDevice : *audioDevice2;
    }

    if (audioDevice && audioDevice2)
    {
        d->audioInfo << new EncodedAudioInfo(this); // second channel
        d->audioInfo[1]->m_audioDevice = *audioDevice2;
    }

    using AudioDeviceChangeNotifier = nx::vms::client::desktop::AudioDeviceChangeNotifier;
    const auto stopOnDeviceDisappeared =
        [this](const QString& deviceName)
        {
            const bool deviceInUse = std::any_of(std::cbegin(d->audioInfo), std::cend(d->audioInfo),
                [&deviceName](const auto& audioInfo)
                {
                    return audioInfo->m_audioDevice.name() == deviceName;}
                );

            if (deviceInUse)
                pleaseStop();
        };

    connect(d->audioDeviceChangeNotifier.get(), &AudioDeviceChangeNotifier::deviceUnplugged,
        this, stopOnDeviceDisappeared, Qt::DirectConnection);

    connect(d->audioDeviceChangeNotifier.get(), &AudioDeviceChangeNotifier::deviceNotPresent,
        this, stopOnDeviceDisappeared, Qt::DirectConnection);

    m_needStop = false;
    d->timer = std::make_unique<QElapsedTimer>();
}

DesktopDataProvider::~DesktopDataProvider()
{
    stop();
}

int DesktopDataProvider::calculateBitrate(const char* codecName)
{
    double bitrate = BASE_BITRATE;

    bitrate /=  1920.0*1080.0 / d->grabber->width() / d->grabber->height();

    bitrate *= d->encodeQualuty;
    if (d->grabber->width() <= 320)
        bitrate *= 1.5;
    if (strcmp(codecName, "libopenh264") == 0)
    {
        // Increase bitrate due to bad quality of libopenh264 coding.
        bitrate *= 4;
    }
    return bitrate;
}

bool DesktopDataProvider::initVideoCapturing()
{
    d->grabber = new BufferedScreenGrabber(
        d->desktopNum,
        BufferedScreenGrabber::DEFAULT_QUEUE_SIZE,
        BufferedScreenGrabber::DEFAULT_FRAME_RATE,
        d->captureMode,
        d->captureCursor,
        d->captureResolution,
        d->widget);
    d->grabber->setLogo(d->logo);
    d->grabber->setTimer(d->timer.get());

    d->frame = av_frame_alloc();
    int ret = av_image_alloc(d->frame->data, d->frame->linesize,
        d->grabber->width(), d->grabber->height(), d->grabber->format(), /*align*/ 1);
    if (ret < 0)
    {
        m_lastErrorStr = tr("Could not detect capturing resolution");
        return false;
    }

    const AVCodec* videoCodec = avcodec_find_encoder_by_name(d->videoCodec.toLatin1().data());
    if (!videoCodec)
    {
        NX_WARNING(this, "Configured codec: %1 not found, h263p will used", d->videoCodec);
        videoCodec = avcodec_find_encoder(AV_CODEC_ID_H263P);
    }
    if (videoCodec == 0)
    {
        m_lastErrorStr = tr("Could not find video encoder %1.").arg(d->videoCodec);
        return false;
    }

    if (d->grabber->width() % 8 != 0) {
        m_lastErrorStr = tr("Screen width must be a multiple of 8.");
        return false;
    }

    d->videoCodecCtx = avcodec_alloc_context3(videoCodec);
    d->videoCodecCtx->codec_id = videoCodec->id;
    d->videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO ;
    d->videoCodecCtx->thread_count = QThread::idealThreadCount();
    d->videoCodecCtx->time_base = d->grabber->getFrameRate();
    d->videoCodecCtx->pix_fmt = d->grabber->format();
    d->videoCodecCtx->coded_width = d->videoCodecCtx->width = d->grabber->width();
    d->videoCodecCtx->coded_height = d->videoCodecCtx->height = d->grabber->height();
    d->videoCodecCtx->bit_rate = calculateBitrate(videoCodec->name);

    if (d->encodeQualuty == 1)
    {
        d->videoCodecCtx->has_b_frames = 1;
        d->videoCodecCtx->level = 50;
        d->videoCodecCtx->gop_size = 25;
        d->videoCodecCtx->trellis = 2;
        d->videoCodecCtx->flags |= AV_CODEC_FLAG_AC_PRED;
        d->videoCodecCtx->flags |= AV_CODEC_FLAG_4MV;
    }

    if (d->captureResolution.width() > 0)
    {
        double srcAspect = d->grabber->screenWidth() / (double) d->grabber->screenHeight();
        double dstAspect = d->captureResolution.width() / (double) d->captureResolution.height();
        double sar = srcAspect / dstAspect;
        d->videoCodecCtx->sample_aspect_ratio = av_d2q(sar, 255);
    }
    else {
        d->videoCodecCtx->sample_aspect_ratio.num = 1;
        d->videoCodecCtx->sample_aspect_ratio.den = 1;
    }

    const auto codecParameters = new CodecParameters(d->videoCodecCtx);
    d->videoContext = CodecParametersConstPtr(codecParameters);

    nx::media::ffmpeg::AvOptions options;
    options.set("motion_est", "epzs", 0);
    if (avcodec_open2(d->videoCodecCtx, videoCodec, options) < 0)
    {
        m_lastErrorStr = tr("Could not initialize video encoder.");
        return false;
    }

    // CodecParameters was created before open() call to avoid encoder specific fields.
    // Transfer extradata manually.
    codecParameters->setExtradata(d->videoCodecCtx->extradata, d->videoCodecCtx->extradata_size);

    return true;
}

bool DesktopDataProvider::initAudioCapturing()
{
    d->initTime = AV_NOPTS_VALUE;

    // init audio capture
    if (!d->audioInfo.isEmpty())
    {
        foreach(EncodedAudioInfo* audioChannel, d->audioInfo)
        {
            if (!audioChannel->setupFormat(m_lastErrorStr))
                return false;
        }

        const auto& format = d->audioInfo.at(0)->m_audioFormat;
        int sampleRate = format.sampleRate();
        int channels = d->audioInfo.size() > 1 ? /*stereo*/ 2 : format.channelCount();

        if (!initAudioEncoder(sampleRate, fromQtAudioFormat(format), channels))
        {
            m_lastErrorStr = tr("Could not initialize audio encoder.");
            return false;
        }

        d->frameSize = getAudioFrameSize();
        d->audioFrameDuration = d->frameSize / (double) sampleRate * 1000; // keep in ms

        d->soundAnalyzer = appContext()->voiceSpectrumAnalyzer();
        d->soundAnalyzer->initialize(sampleRate, channels);

        foreach(EncodedAudioInfo* audioChannel, d->audioInfo)
        {
            if (!audioChannel->setupPostProcess(d->frameSize, channels, sampleRate))
            {
                m_lastErrorStr = tr("Could not initialize audio device \"%1\".").arg(audioChannel->m_audioDevice.fullName());
                return false;
            }
        }
    }
    else
    {
        NX_DEBUG(this, "Input audio device is not selected.");
    }

    // 50 ms as max jitter.
    // Qt uses 25fps timer for audio grabbing, so jitter 40ms + 10ms reserved.
    d->maxAudioJitter = 1000 / 20; //< Keep in ms.

    foreach (EncodedAudioInfo* info, d->audioInfo)
    {
        if (!info->start())
        {
            m_lastErrorStr = tr("Could not start primary audio device.");
            return false;
        }
    }

    return true;
}

int DesktopDataProvider::processData(bool flush)
{
    if (d->videoCodecCtx == 0)
        return -1;
    if (d->frame->width <= 0 || d->frame->height <= 0)
        return -1;

    nx::media::ffmpeg::AvPacket avPacket;
    auto packet = avPacket.get();
    int got_packet = 0;
    int encodeResult = nx::media::ffmpeg::old_api::encode(
        d->videoCodecCtx, packet, flush ? 0 : d->frame, &got_packet);

    if (encodeResult < 0)
        return encodeResult; //< error

    AVRational timeBaseNative;
    timeBaseNative.num = 1;
    timeBaseNative.den = 1000000;

    if (d->initTime == AV_NOPTS_VALUE)
        d->initTime = qnSyncTime->currentUSecsSinceEpoch();

    if (got_packet > 0)
    {
        QnWritableCompressedVideoDataPtr video = QnWritableCompressedVideoDataPtr(
            new QnWritableCompressedVideoData(packet->size, d->videoContext));
        video->m_data.write((const char*) packet->data, packet->size);
        video->compressionType = d->videoCodecCtx->codec_id;
        video->timestamp = av_rescale_q(packet->pts, d->videoCodecCtx->time_base, timeBaseNative) + d->initTime;

        if(packet->flags & AV_PKT_FLAG_KEY)
            video->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        video->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        video->dataProvider = this;
        putData(video);
    }

    putAudioData();

    return packet->size;
}

void DesktopDataProvider::putAudioData()
{
    // write all audio frames
    EncodedAudioInfo* ai = d->audioInfo.size() > 0 ? d->audioInfo[0] : 0;
    EncodedAudioInfo* ai2 = d->audioInfo.size() > 1 ? d->audioInfo[1] : 0;
    while (ai && ai->m_audioQueue.size() > 0 && (ai2 == 0 || ai2->m_audioQueue.size() > 0))
    {
        QnWritableCompressedAudioDataPtr audioData;
        ai->m_audioQueue.pop(audioData);

        qint64 audioPts = audioData->timestamp - d->audioFrameDuration;
        qint64 expectedAudioPts = d->storedAudioPts + d->audioFramesCount * d->audioFrameDuration;
        int audioJitter = qAbs(audioPts - expectedAudioPts);

        if (audioJitter < d->maxAudioJitter)
        {
            audioPts = expectedAudioPts;
        }
        else {
            d->storedAudioPts = audioPts;
            d->audioFramesCount = 0;
        }

        d->audioFramesCount++;

        // todo: add audio resample here

        short* buffer1 = (short*) audioData->m_data.data();
        if (ai->m_speexPreprocess)
            speex_preprocess(ai->m_speexPreprocess, buffer1, nullptr);

        if (ai2)
        {
            QnWritableCompressedAudioDataPtr audioData2;
            ai2->m_audioQueue.pop(audioData2);
            short* buffer2 = (short*) audioData2->m_data.data();
            if (ai2->m_speexPreprocess)
                speex_preprocess(ai2->m_speexPreprocess, buffer2, nullptr);

            int stereoPacketSize = d->frameSize * 2 * ai->m_audioFormat.bytesPerSample();
            /*
            // first mono to left, second mono to right
            // may be it is mode useful?
            if (d->audioFormat.channels() == 1 && d->audioFormat2.channels() == 1)
            {
                monoToStereo((qint16*) d->tmpAudioBuffer1.data.data(), buffer1, buffer2, stereoPacketSize/4);
                buffer1 = (qint16*) d->tmpAudioBuffer1.data.data();
                buffer2 = 0;
            }
            */
            if (ai->m_audioFormat.channelCount() == 1)
            {
                monoToStereo((qint16*) ai->m_tmpAudioBuffer.m_data.data(), buffer1, stereoPacketSize/4);
                buffer1 = (qint16*) ai->m_tmpAudioBuffer.m_data.data();
            }
            if (ai2->m_audioFormat.channelCount() == 1)
            {
                monoToStereo((qint16*) ai2->m_tmpAudioBuffer.m_data.data(), buffer2, stereoPacketSize/4);
                buffer2 = (qint16*) ai2->m_tmpAudioBuffer.m_data.data();
            }
            if (buffer2)
                stereoAudioMux(buffer1, buffer2, stereoPacketSize / 2);
        }
        d->soundAnalyzer->processData(buffer1, d->frameSize);

        int audioChannelNumber = needVideoData() ? 1 : 0;
        if (!encodeAndPutAudioData((uint8_t*)buffer1, d->frameSize, audioChannelNumber))
            return;
    }
}

void DesktopDataProvider::start(Priority priority)
{
    NX_MUTEX_LOCKER lock( &d->startMutex );
    if (d->started)
        return;
    d->started = true;

    d->isAudioInitialized = initAudioCapturing();
    if (!d->isAudioInitialized)
    {
        m_needStop = true;
        NX_WARNING(this, "Could not initialize audio capturing: %1", m_lastErrorStr);
    }

    QnLongRunnable::start(priority);
}

bool DesktopDataProvider::isInitialized() const
{
    return d->isAudioInitialized;
}

bool DesktopDataProvider::needVideoData() const
{
    const auto dataProcessors = m_dataprocessors.lock();
    for (const auto& consumer: *dataProcessors)
    {
        if (consumer->needConfigureProvider())
            return true;
    }
    return false;
}

void DesktopDataProvider::run()
{
    QThread::currentThread()->setPriority(QThread::HighPriority);
    d->timer->restart();

    while (!needToStop() || (d->grabber && d->grabber->dataExist()))
    {
        if (needVideoData())
        {
            if (!d->isVideoInitialized)
            {
                d->isVideoInitialized = initVideoCapturing();
                if (!d->isVideoInitialized)
                {
                    m_needStop = true;
                    break;
                }
            }

            d->grabber->start(QThread::HighestPriority);

            if (needToStop() && !d->capturingStopped)
            {
                stopCapturing();
                d->capturingStopped = true;
            }

            CaptureInfoPtr capturedData = d->grabber->getNextFrame();
            if (!capturedData || !capturedData->opaque || capturedData->width == 0 || capturedData->height == 0)
                continue;
            d->grabber->capturedDataToFrame(capturedData, d->frame);

            AVRational r;
            r.num = 1;
            r.den = 1000;
            qint64 capturedPts = av_rescale_q(d->frame->pts, r, d->grabber->getFrameRate());
            if (d->encodedFrames == 0)
                d->encodedFrames = capturedPts;

            bool firstStep = true;
            while (firstStep || capturedPts - d->encodedFrames >= MAX_VIDEO_JITTER)
            {
                d->frame->pts = d->encodedFrames;
                if (processData(false) < 0)
                {
                    NX_WARNING(this, "Video encoding error. Stop recording.");
                    break;
                }

                d->encodedFrames++;
                firstStep = false;
            }
        }
        else
        {
            if (d->grabber)
                d->grabber->pleaseStop();
            putAudioData();
        }
        if (needToStop())
            break;
    }
    if (!d->capturingStopped)
        stopCapturing();

    NX_VERBOSE(this, "flushing video buffer");
    if (needVideoData())
    {
        do {
        } while (processData(true) > 0); // flush buffers
    }

    closeStream();
}

void DesktopDataProvider::stopCapturing()
{
    foreach(EncodedAudioInfo* info, d->audioInfo)
        info->stop();
    if (d->grabber)
        d->grabber->pleaseStop();
}

void DesktopDataProvider::closeStream()
{
    delete d->grabber;
    d->grabber = 0;

    avcodec_free_context(&d->videoCodecCtx);

    if (d->frame) {
        av_freep(&d->frame->data[0]);
        av_free(d->frame);
        d->frame = 0;
    }

    foreach(EncodedAudioInfo* audioChannel, d->audioInfo)
        delete audioChannel;
    d->audioInfo.clear();
}

qint64 DesktopDataProvider::currentTime() const
{
    return d->timer->elapsed();
}

int DesktopDataProvider::frameSize() const
{
    return d->frameSize;
}

void DesktopDataProvider::removeDataProcessor(QnAbstractMediaDataReceptor* consumer)
{
    NX_MUTEX_LOCKER lock( &d->startMutex );
    QnAbstractStreamDataProvider::removeDataProcessor(consumer);
    if (processorsCount() == 0)
        pleaseStop();
}

void DesktopDataProvider::addDataProcessor(QnAbstractMediaDataReceptor* consumer)
{
    NX_MUTEX_LOCKER lock( &d->startMutex );
    if (needToStop()) {
        wait(); // wait previous thread instance
        d->started = false; // now we ready to restart
    }
    QnAbstractStreamDataProvider::addDataProcessor(consumer);
}

bool DesktopDataProvider::readyToStop() const
{
    NX_MUTEX_LOCKER lock(&d->startMutex);
    return processorsCount() == 0;
}

} // namespace nx::vms::client::desktop
