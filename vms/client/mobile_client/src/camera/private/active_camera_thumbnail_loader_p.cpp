// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_camera_thumbnail_loader_p.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/media/jpeg.h>
#include <nx/utils/guarded_callback.h>

namespace {

constexpr int refreshInterval = 300;
constexpr int fastTimeout = 100;
constexpr int longTimeout = 230;
constexpr int invalidRequest = -1;

} // namespace

QnActiveCameraThumbnailLoaderPrivate::QnActiveCameraThumbnailLoaderPrivate(
    QnActiveCameraThumbnailLoader* parent)
    :
    QObject(parent),
    q_ptr(parent),
    refreshOperation(
        new nx::utils::PendingOperation(
            [this]{ refresh(); },
            refreshInterval,
            this)),
    requestId(invalidRequest),
    decompressThread(new QThread(this))
{
    screenshotQualityList.append(128);
    screenshotQualityList.append(240);
    screenshotQualityList.append(320);
    screenshotQualityList.append(480);
    screenshotQualityList.append(560);
    screenshotQualityList.append(720);
    screenshotQualityList.append(1080);

    decompressThread->setObjectName("QnActiveCameraThumbnailLoader_decompressThread");
    decompressThread->start();
}

QnActiveCameraThumbnailLoaderPrivate::~QnActiveCameraThumbnailLoaderPrivate()
{
    decompressThread->quit();
    decompressThread->wait();
}

QPixmap QnActiveCameraThumbnailLoaderPrivate::thumbnail() const
{
    QMutexLocker lk(&thumbnailMutex);
    return thumbnailPixmap;
}

void QnActiveCameraThumbnailLoaderPrivate::clear()
{
    QMutexLocker lk(&thumbnailMutex);
    thumbnailPixmap = QPixmap();
    thumbnailId = QString();
}

void QnActiveCameraThumbnailLoaderPrivate::refresh(bool force)
{
    if (force)
        requestId = invalidRequest;

    if (requestId != invalidRequest)
    {
        requestNextAfterReply = true;
        return;
    }

    if (!camera)
        return;

    if (!connection())
        return;

    using namespace std::chrono;

    request.camera = camera;
    request.timestampMs = milliseconds(position);
    request.size = currentSize();

    const auto handleReply = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle handleId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            if (requestId != handleId)
                return;

            requestId = invalidRequest;

            if (currentQuality < screenshotQualityList.size() - 1 && !fetchTimer.hasExpired(fastTimeout))
                ++currentQuality;
            else if (currentQuality > 0 && fetchTimer.hasExpired(longTimeout))
                --currentQuality;

            if (requestNextAfterReply)
            {
                requestNextAfterReply = false;
                refresh();
            }

            if (!success || imageData.isEmpty())
                return;

            {
                QMutexLocker lk(&thumbnailMutex);
                thumbnailPixmap = QPixmap::fromImage(decompressJpegImage(imageData));
            }

            Q_Q(QnActiveCameraThumbnailLoader);
            if (camera)
            {
                thumbnailId =
                    NX_FMT("%1/%2", camera->getId().toString(QUuid::WithBraces), position);
                emit q->thumbnailIdChanged();
            }
        });

    requestId = connectedServerApi()->cameraThumbnailAsync(request, handleReply, decompressThread);
    fetchTimer.start();
}

void QnActiveCameraThumbnailLoaderPrivate::requestRefresh()
{
    refreshOperation->requestOperation();
}

QSize QnActiveCameraThumbnailLoaderPrivate::currentSize() const
{
    return QSize(0, screenshotQualityList[currentQuality]);
}
