// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef _WIN32

#include <d3d9.h>
#include <dxva2api.h>
#include <wrl/client.h>

#include <vpl/mfx.h>

#include "renderer.h"

namespace nx::media::quick_sync::windows {

class DeviceHandle
{
public:
    ~DeviceHandle();
    bool createDevice(int width, int height, mfxU32 adapterNumber);

    IDirect3DDeviceManager9* getDeviceManager() const { return m_deviceManager.Get(); }
    Renderer& getRenderer() { return m_renderer; }

private:
    Microsoft::WRL::ComPtr<IDirect3DDeviceManager9> m_deviceManager;
    Microsoft::WRL::ComPtr<IDirect3D9Ex> m_d3d;
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> m_device;

    HWND m_hWnd = 0;
    Renderer m_renderer;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
