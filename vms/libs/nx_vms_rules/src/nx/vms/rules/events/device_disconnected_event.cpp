// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_disconnected_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/source_camera_field.h"
#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

DeviceDisconnectedEvent::DeviceDisconnectedEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid deviceId)
    :
    BasicEvent(timestamp),
    m_deviceId(deviceId)
{
}

QVariantMap DeviceDisconnectedEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForDevice(result, context, deviceId(), detailLevel);

    result[utils::kCaptionDetailName] = caption(context);
    result[utils::kNameDetailName] = name(context);
    utils::insertIcon(result, nx::vms::rules::Icon::resource);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::cameraSettings);

    return result;
}

QString DeviceDisconnectedEvent::caption(common::SystemContext* context) const
{
    const auto camera = context->resourcePool()->getResourceById<QnVirtualCameraResource>(
        deviceId());

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device was disconnected"),
            tr("Camera was disconnected"),
            tr("I/O Module was disconnected")),
        camera);
}

QString DeviceDisconnectedEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, deviceId(), detailLevel);
    return tr("%1 was disconnected", "Device name will be substituted").arg(resourceName);
}

QString DeviceDisconnectedEvent::name(common::SystemContext* context) const
{
    return manifest(context).displayName();
}

ItemDescriptor DeviceDisconnectedEvent::manifest(common::SystemContext* context)
{
    auto getDisplayName =
        [context = QPointer<common::SystemContext>(context)]
        {
            if (!context)
                return QString();

            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("Device Disconnected"),
                tr("Camera Disconnected"));
        };

    auto getSourceName =
        [context = QPointer<common::SystemContext>(context)]
        {
            if (!context)
                return QString();

            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("Device"),
                tr("Camera"));
        };

    auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceDisconnectedEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = TranslatableString(getDisplayName),
        .description = "Triggered when a device is disconnected, regardless of the cause.",
        .flags = {ItemFlag::instant},
        .fields = {
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
                TranslatableString(getSourceName),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .acceptAll = true,
                    .allowEmptySelection = true
                }.toVariantMap()),
        },
        .resources = {
            {utils::kDeviceIdFieldName, {ResourceType::device, Qn::ViewContentPermission}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
