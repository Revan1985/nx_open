// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVariantMap>

#include <nx/vms/event/level.h>
#include <nx/vms/rules/client_action.h>
#include <nx/vms/rules/icon.h>

namespace nx::vms::rules::utils {

// TODO: #amalov Think of passing ResourceInfoLevel & AttrSerializePolicy to details() method.

static constexpr auto kAnalyticsEventTypeDetailName = "analyticsEventType";
static constexpr auto kAnalyticsObjectTypeDetailName = "analyticsObjectType";
static constexpr auto kCaptionDetailName = "caption";
static constexpr auto kClientActionDetailName = "clientAction";
static constexpr auto kCustomIconDetailName = "customIcon";
static constexpr auto kDescriptionDetailName = "description";
static constexpr auto kDestinationDetailName = "destination";
static constexpr auto kDetailingDetailName = "detailing";

// Verbose event caption with resource name. Used as email subject or bookmark name.
static constexpr auto kExtendedCaptionDetailName = "extendedCaption";

// Separate 'Caption' line in notification tooltip.
static constexpr auto kExtraCaptionDetailName = "extraCaption";
static constexpr auto kIconDetailName = "icon";
static constexpr auto kLevelDetailName = "level";
static constexpr auto kNameDetailName = "name";
static constexpr auto kPluginIdDetailName = "pluginId";
static constexpr auto kReasonDetailName = "reason";
static constexpr auto kSourceIdDetailName = "sourceId";
static constexpr auto kSourceNameDetailName = "sourceName";
static constexpr auto kTypeDetailName = "type";
static constexpr auto kUrlDetailName = "url";
static constexpr auto kUserIdDetailName = "userId";

template <class T>
void insertIfNotEmpty(QVariantMap& map, const QString& key, const T& value)
{
    if (!value.isEmpty())
        map.insert(key, value);
}

inline void insertIfValid(QVariantMap& map, const QString& key, const QVariant& value)
{
    if (value.isValid())
        map.insert(key, value);
}

inline void insertLevel(QVariantMap& map, nx::vms::event::Level level)
{
    map.insert(kLevelDetailName, QVariant::fromValue(level));
}

inline void insertIcon(QVariantMap& map, nx::vms::rules::Icon icon)
{
    map.insert(kIconDetailName, QVariant::fromValue(icon));
}

inline void insertClientAction(QVariantMap& map, nx::vms::rules::ClientAction action)
{
    map.insert(kClientActionDetailName, QVariant::fromValue(action));
}

} // namespace nx::vms::rules::utils
