// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multicast_address_registry.h"

#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>

namespace nx::network {

MulticastAddressRegistry::RegisteredAddressHolderPtr MulticastAddressRegistry::registerAddress(
    QnVirtualCameraResourcePtr resource,
    nx::vms::api::StreamIndex streamIndex,
    nx::network::SocketAddress address)
{
    NX_DEBUG(this, "Registering a multicast address %1, resource %2", address, resource);
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_registry.find(address);

    if (it != m_registry.cend())
    {
        const auto& usageInfo = it->second;
        NX_WARNING(this,
            "Multicast address %1 is already registered by resource %2, on stream: %3",
            address, usageInfo.device.toStrongRef(), usageInfo.stream);
        return nullptr;
    }

    NX_DEBUG(this, "Successfully registered a multicast address %1 for resource %2 on stream %3",
        address, resource, streamIndex);
    m_registry[address] = { resource.toWeakRef(), streamIndex };
    return std::make_unique<RegisteredAddressHolder>(
        [this, address = std::move(address)]() { unregisterAddress(std::move(address)); });
}

bool MulticastAddressRegistry::unregisterAddress(const nx::network::SocketAddress& address)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_DEBUG(this, "Unregistering a multicast address %1", address);
    const auto it = m_registry.find(address);
    if (it == m_registry.cend())
    {
        NX_DEBUG(this, "Multicast address %1 is not registered", address);
        return false;
    }

    NX_DEBUG(this, "Multicast address %1 has been successfully unregistered", address);
    m_registry.erase(address);
    return true;
}

MulticastAddressRegistry::AddressUsageInfo MulticastAddressRegistry::addressUsageInfo(
    const nx::network::SocketAddress& address) const
{
    auto it = m_registry.find(address);
    if (it != m_registry.cend())
        return it->second;

    return it->second;
}

} // namespace nx::network
