// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/ordered_requests_helper.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_streamer.h>

namespace nx::vms::client::core {

struct TwoWayAudioController::Private
{
    Private(TwoWayAudioController* q);
    void setStarted(bool value);
    bool setActive(bool active, OperationCallback&& callback = OperationCallback());

    TwoWayAudioController* const q;
    OrderedRequestsHelper orderedRequestsHelper;
    QScopedPointer<TwoWayAudioAvailabilityWatcher> availabilityWatcher;
    std::shared_ptr<TwoWayAudioStreamer> streamer;
    QString sourceId;
    bool started = false;
    bool available = false;
};

TwoWayAudioController::Private::Private(TwoWayAudioController* q):
    q(q),
    availabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        q, &TwoWayAudioController::availabilityChanged);
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::audioOutputDeviceChanged, q,
        [q]() { q->stop(); });
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::cameraChanged,
        q, &TwoWayAudioController::resourceIdChanged);
}

void TwoWayAudioController::Private::setStarted(bool value)
{
    if (started == value)
        return;

    started = value;
    emit q->startedChanged();
}

bool TwoWayAudioController::Private::setActive(bool active, OperationCallback&& callback)
{
    const bool available = q->connection() && q->available();
    setStarted(active && available);
    if (!available)
        return false;

    const auto targetResource = availabilityWatcher->audioOutputDevice();
    if (!NX_ASSERT(targetResource))
        return false;

    if (active)
    {
        auto desktop = q->resourcePool()->getResourceById<DesktopResource>(
            DesktopResource::getDesktopResourceUuid());

        if (!desktop)
        {
            NX_WARNING(this, "Desktop resource not found");
            return false;
        }
        std::unique_ptr<QnAbstractStreamDataProvider> provider(
            desktop->createDataProvider(Qn::CR_Default));
        if (!provider)
        {
            NX_WARNING(this, "Failed to create desktop data provider");
            return false;
        }
        streamer = std::make_shared<TwoWayAudioStreamer>(std::move(provider));
        const bool result = streamer->start(q->connection()->credentials(), targetResource);
        setStarted(result);
        if (callback)
            callback(result);
    }
    else
    {
        streamer.reset();
        setStarted(false);
        if (callback)
            callback(true);
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

TwoWayAudioController::TwoWayAudioController(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
}

TwoWayAudioController::~TwoWayAudioController()
{
    stop();
}

void TwoWayAudioController::registerQmlType()
{
    qmlRegisterUncreatableType<TwoWayAudioController>(
        "nx.vms.client.core", 1, 0, "TwoWayAudioController",
        "Cannot create an instance of TwoWayAudioController without a system context.");
}

bool TwoWayAudioController::started() const
{
    return d->started;
}

bool TwoWayAudioController::start(OperationCallback&& callback)
{
    return !started() && d->setActive(true, std::move(callback));
}

bool TwoWayAudioController::stop(OperationCallback&& callback)
{
    return started() && d->setActive(false, std::move(callback));
}

void TwoWayAudioController::setSourceId(const QString& value)
{
    if (d->sourceId == value)
        return;

    if (d->sourceId.isEmpty())
        stop();

    d->sourceId = value;
}

nx::Uuid TwoWayAudioController::resourceId() const
{
    const auto camera = d->availabilityWatcher->camera();
    return camera ? camera->getId() : nx::Uuid();
}

void TwoWayAudioController::setResourceId(const nx::Uuid& id)
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    d->availabilityWatcher->setCamera(camera);
}

void TwoWayAudioController::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->availabilityWatcher->setCamera(camera);
}

bool TwoWayAudioController::available() const
{
    return d->availabilityWatcher->available();
}

} // namespace nx::vms::client::core
