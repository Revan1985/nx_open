// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

EventSearchUtils::EventSearchUtils(QObject* parent):
    base_type(parent)
{
}

QString EventSearchUtils::timeSelectionText(EventSearch::TimeSelection value)
{
    switch (value)
    {
        case EventSearch::TimeSelection::day:
            return tr("Last day");
        case EventSearch::TimeSelection::week:
            return tr("Last 7 days");
        case EventSearch::TimeSelection::month:
            return tr("Last 30 days");
        default:
            return tr("Any time");
    }
}

QString EventSearchUtils::cameraSelectionText(
    EventSearch::CameraSelection selection,
    const QnVirtualCameraResourceSet& cameras)
{
    switch (selection)
    {
        case EventSearch::CameraSelection::all:
            return tr("Any");
        case EventSearch::CameraSelection::custom:
            return cameras.size() == 1
                ? (*cameras.begin())->getName()
                : tr("%n Cameras","%n is a number of cameras", cameras.size());
        default:
            return {};
    }
}

QString EventSearchUtils::cameraSelectionText(
    EventSearch::CameraSelection selection,
    const UuidList& cameraIds,
    SystemContext* context)
{
    QnVirtualCameraResourceSet cameras;
    const auto pool = context->resourcePool();
    for (const auto& id: cameraIds)
    {
        if (const auto& camera = pool->getResourceById<QnVirtualCameraResource>(id))
            cameras.insert(camera);
    }

    return cameraSelectionText(selection, cameras);
}

FetchRequest EventSearchUtils::fetchRequest(
    EventSearch::FetchDirection direction,
    qint64 centralPointUs)
{
    return FetchRequest{
        .direction = direction,
        .centralPointUs = std::chrono::microseconds(centralPointUs)};
}

QString EventSearchUtils::timestampText(
    qint64 timestampMs,
    SystemContext* context) const
{
    return timestampText(std::chrono::milliseconds(timestampMs), context);
}

QString EventSearchUtils::timeFromNowText(
    const std::chrono::microseconds& timestamp, SystemContext* context)
{
    using namespace std::chrono;
    if (timestamp <= 0ms || !NX_ASSERT(context))
        return {};

    const auto timestampMs = duration_cast<milliseconds>(timestamp);
    const QDateTime dateTime = context->serverTimeWatcher()->displayTime(timestampMs.count());

    // For current day display time in the format '<N> <Time> ago'.
    if (qnSyncTime->currentDateTime().date() == dateTime.date())
    {
        const milliseconds msecsAgo{
            QDateTime::currentMSecsSinceEpoch() - dateTime.toMSecsSinceEpoch()};
        return time::fromNow(duration_cast<seconds>(msecsAgo)).toUpper();
    }

    return timestampText(timestampMs, context);
}


QString EventSearchUtils::timestampText(
    const std::chrono::milliseconds& timestampMs,
    SystemContext* context)
{
    using namespace std::chrono;
    if (timestampMs <= 0ms || !NX_ASSERT(context))
        return {};

    const QDateTime dateTime = context->serverTimeWatcher()->displayTime(timestampMs.count());

    // For current day just display the time in system format.
    if (qnSyncTime->currentDateTime().date() == dateTime.date())
        return nx::vms::time::toString(dateTime.time());

    // Display both date and time for all other days.
    return nx::vms::time::toString(dateTime, nx::vms::time::Format::dd_MM_yy_hh_mm_ss);
}

} // namespace nx::vms::client::core
