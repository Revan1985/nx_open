// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace events {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(nx::sdk::analytics::Integration* integration);
    virtual ~Engine() override;

    nx::sdk::analytics::Integration* const integration() const { return m_integration; }

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    nx::sdk::analytics::Integration* const m_integration;
    std::atomic<bool> m_terminated{false};
};

} // namespace events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
