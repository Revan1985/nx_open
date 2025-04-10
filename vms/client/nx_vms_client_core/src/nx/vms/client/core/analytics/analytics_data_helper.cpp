// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_data_helper.h"

#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/api/data/user_group_data.h>

namespace nx::vms::client::core {

AnalyticsEngineInfo engineInfoFromResource(
    const common::AnalyticsEngineResourcePtr& engine, SettingsModelSource settingsModelSource)
{
    const auto plugin = engine->getParentResource().dynamicCast<common::AnalyticsPluginResource>();
    if (!plugin)
        return {};

    const auto integrationManifest = plugin->manifest();

    auto settingsModel = settingsModelSource == SettingsModelSource::manifest
        ? engine->manifest().deviceAgentSettingsModel
        : QJson::deserialized<QJsonObject>(
            engine->getProperty(common::AnalyticsEngineResource::kSettingsModelProperty).toUtf8());

    return AnalyticsEngineInfo {
        engine->getId(),
        plugin->getId(),
        engine->getName(),
        integrationManifest.description,
        integrationManifest.version,
        integrationManifest.vendor,
        integrationManifest.isLicenseRequired,
        std::move(settingsModel),
        engine->isDeviceDependent(),
        plugin->integrationType(),

        // Currently all api integrations have Power Users permission.
        plugin->integrationType() == nx::vms::api::analytics::IntegrationType::api
            ? nx::vms::api::kPowerUsersGroupId
            : std::optional<nx::Uuid>{}
    };
}

} // namespace nx::vms::client::core
