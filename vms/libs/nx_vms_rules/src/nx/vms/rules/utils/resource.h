// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>

#include "../rules_fwd.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

enum class ResourceType;
struct UuidSelection;

namespace utils {

NX_VMS_RULES_API QnUserResourceSet users(
    const UuidSelection& selection,
    const common::SystemContext* context,
    bool activeOnly = false);

NX_VMS_RULES_API UuidSet userIds(
    const UuidSelection& selection, const common::SystemContext* context, bool activeOnly);

NX_VMS_RULES_API bool isUserSelected(
    const UuidSelection& selection,
    const common::SystemContext* context,
    nx::Uuid userId);

NX_VMS_RULES_API QnMediaServerResourceList servers(
    const UuidSelection& selection,
    const common::SystemContext* context);

NX_VMS_RULES_API QnVirtualCameraResourceList cameras(
    const UuidSelection& selection,
    const common::SystemContext* context);

template <class T>
UuidList fieldResourceIds(const T& entity, std::string_view fieldName)
{
    const auto value = entity->property(fieldName.data());

    if (!value.isValid())
        return {};

    UuidList result;

    if (value.template canConvert<UuidList>())
        result = value.template value<UuidList>();
    else if (value.template canConvert<nx::Uuid>())
        result.emplace_back(value.template value<nx::Uuid>());

    result.removeAll({});

    return result;
}

NX_VMS_RULES_API std::string resourceField(const ItemDescriptor& manifest, ResourceType type);

NX_VMS_RULES_API UuidList getDeviceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API UuidList getDeviceIds(const EventPtr& event);
NX_VMS_RULES_API UuidList getDeviceIds(const ActionPtr& action);

NX_VMS_RULES_API UuidList getResourceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API UuidList getResourceIds(const ActionPtr& action);

} // namespace utils
} // namespace nx::vms::rules
