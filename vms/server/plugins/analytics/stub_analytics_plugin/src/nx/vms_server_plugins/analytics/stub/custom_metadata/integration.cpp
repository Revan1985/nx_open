// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include "engine.h"
#include "stub_analytics_plugin_custom_metadata_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace custom_metadata {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine(this);
}

std::string Integration::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": ")json" + instanceId() + R"json(",
    "name": "Stub, Custom Metadata",
    "description":
        "A plugin for testing and debugging Camera-generated Custom Metadata.",
    "version": "1.0.0",
    "vendor": "Plugin vendor"
}
)json";
}

} // namespace custom_metadata
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
