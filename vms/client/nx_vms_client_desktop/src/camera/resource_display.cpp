// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_display.h"

#include <cassert>

#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/utils/counter.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/util.h>

using namespace nx::vms::client::desktop;

QnResourceDisplay::QnResourceDisplay(const QnResourcePtr &resource, QObject *parent):
    base_type(parent),
    m_resource(resource),
    m_started(false)
{
    NX_ASSERT(resource);

    m_mediaResource = resource.dynamicCast<QnMediaResource>();

    m_dataProvider = m_mediaResource->createDataProvider();
    if (m_dataProvider)
    {
        m_archiveReader = dynamic_cast<QnAbstractArchiveStreamReader *>(m_dataProvider.data());
        if (m_archiveReader && resource->flags().testFlag(Qn::local_media))
            m_archiveReader->setCycleMode(true);

        m_mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider *>(m_dataProvider.data());

        if (m_mediaProvider != nullptr)
        {
            /* Camera will free media provider in its destructor. */
            m_camera = new QnClientVideoCamera(m_mediaResource, m_mediaProvider);

            connect(this, &QObject::destroyed, m_camera, &QnClientVideoCamera::beforeStopDisplay);

            m_counter = new nx::utils::CounterWithSignal(1);
            connect(m_camera->getCamDisplay(), &QnCamDisplay::finished, m_counter, &nx::utils::CounterWithSignal::decrement);
            if (m_mediaProvider->hasThread())
            {
                connect(m_camera->getStreamreader(), &QnAbstractMediaStreamDataProvider::finished,
                    m_counter, &nx::utils::CounterWithSignal::decrement);
                m_counter->increment();
            }

            connect(m_counter, &nx::utils::CounterWithSignal::reachedZero, m_counter, &QObject::deleteLater);
            connect(m_counter, &nx::utils::CounterWithSignal::reachedZero, m_camera, &QObject::deleteLater);
        }
        else
        {
            m_camera = nullptr;

            connect(this, &QObject::destroyed, m_dataProvider,
                &QnAbstractStreamDataProvider::pleaseStop);
            connect(m_dataProvider, &QnAbstractStreamDataProvider::finished,
                m_dataProvider, &QObject::deleteLater);
        }
    }
}

QnResourceDisplay::~QnResourceDisplay()
{
    if (m_camera && !m_camera->isDisplayStarted())
    {
        if (m_counter)
            m_counter->deleteLater();
        m_camera->deleteLater();
    }

    disconnectFromResource();
}

void QnResourceDisplay::beforeDestroy()
{
}

const QnResourcePtr& QnResourceDisplay::resource() const
{
    return m_resource;
}

const QnMediaResourcePtr& QnResourceDisplay::mediaResource() const
{
    return m_mediaResource;
}

QnAbstractStreamDataProvider* QnResourceDisplay::dataProvider() const
{
    return m_dataProvider.data();
}

QnAbstractMediaStreamDataProvider* QnResourceDisplay::mediaProvider() const
{
    return m_mediaProvider.data();
}

QnAbstractArchiveStreamReader* QnResourceDisplay::archiveReader() const
{
    return m_archiveReader.data();
}

QnClientVideoCamera* QnResourceDisplay::camera() const
{
    return m_camera.data();
}

void QnResourceDisplay::cleanUp(QnLongRunnable* runnable) const
{
    if (runnable == nullptr)
        return;

    auto context = m_resource ? m_resource->systemContext() : nullptr;
    nx::vms::common::appContext()->longRunableCleanup()->cleanupAsync(
        std::unique_ptr<QnLongRunnable>(runnable), context);
}

QnCamDisplay *QnResourceDisplay::camDisplay() const {
    if(m_camera == nullptr)
        return nullptr;

    return m_camera->getCamDisplay();
}

void QnResourceDisplay::disconnectFromResource()
{
    if (!m_dataProvider)
        return;

    if (m_mediaProvider && m_camera)
        m_mediaProvider->removeDataProcessor(m_camera->getCamDisplay());

    cleanUp(m_dataProvider);
    if (m_camera)
        m_camera->beforeStopDisplay();

    m_mediaResource.clear();
    m_dataProvider.clear();
    m_mediaProvider.clear();
    m_archiveReader.clear();
    m_camera.clear();
}

QnConstResourceVideoLayoutPtr QnResourceDisplay::videoLayout() const {
    if (!m_mediaProvider)
        return QnConstResourceVideoLayoutPtr();

    if (!m_mediaResource)
        return QnConstResourceVideoLayoutPtr();

    return m_mediaResource->getVideoLayout(m_mediaProvider);
}

qint64 QnResourceDisplay::lengthUSec() const {
    return m_archiveReader == nullptr ? -1 : m_archiveReader->lengthUsec();
}

qint64 QnResourceDisplay::currentTimeUSec() const
{
    if (!m_camera)
        return -1;

    if (m_camera->getCamDisplay()->isRealTimeSource())
        return DATETIME_NOW;

    return m_camera->getCamDisplay()->getCurrentTime();
}

void QnResourceDisplay::start() {
    m_started = true;

    if(m_camera != nullptr)
        m_camera->startDisplay();
}

void QnResourceDisplay::play() {
    if(m_archiveReader == nullptr)
        return;

    m_archiveReader->resumeMedia();

    //if (m_graphicsWidget->isSelected() || !m_playing)
    //    m_camera->getCamDisplay()->playAudio(m_playing);
}

void QnResourceDisplay::pause() {
    if (!m_archiveReader)
        return;

    m_archiveReader->pauseMedia();
}

bool QnResourceDisplay::isPaused() {
    if (!m_archiveReader)
        return false;

    return m_archiveReader->isMediaPaused();
}

bool QnResourceDisplay::isStillImage() const {
    return m_resource->flags() & Qn::still_image;
}

void QnResourceDisplay::addRenderer(QnResourceWidgetRenderer *renderer) {
    if (m_camera)
        m_camera->getCamDisplay()->addVideoRenderer(videoLayout()->channelCount(), renderer, true);
}

void QnResourceDisplay::removeRenderer(QnResourceWidgetRenderer* renderer)
{
    if (m_camera)
        m_camera->getCamDisplay()->removeVideoRenderer(renderer);
}

void QnResourceDisplay::addMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camera)
        m_camera->getCamDisplay()->addMetadataConsumer(metadataConsumer);
}

void QnResourceDisplay::removeMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (m_camera)
        m_camera->getCamDisplay()->removeMetadataConsumer(metadataConsumer);
}

QString QnResourceDisplay::codecName() const
{
    if (m_camera)
        return m_camera->getCamDisplay()->codecName();

    return QString();
}
