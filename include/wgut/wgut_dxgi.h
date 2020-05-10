#pragma once
#include "wgut_common.h"
#include <vector>
#include <d3d11.h>
#include <dxgi1_3.h>

namespace wgut::dxgi
{

inline std::vector<ComPtr<IDXGIAdapter>> Adapters()
{
    std::vector<ComPtr<IDXGIAdapter>> adapters;

    ComPtr<IDXGIFactory> factory;
    if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
    {
        ComPtr<IDXGIAdapter> adapter;
        for (UINT i = 0;
             factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            adapters.push_back(adapter);
        }
    }

    return adapters;
}

inline ComPtr<IDXGISwapChain1> CreateSwapChain(ComPtr<ID3D11Device> device, HWND hwnd)
{
    ComPtr<IDXGIDevice3> dxgiDevice;
    if (FAILED(device.As(&dxgiDevice)))
    {
        return nullptr;
    }

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter)))
    {
        return nullptr;
    }

    ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
    {
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{
        // UINT Width;
        // UINT Height;
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1, 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        // DXGI_SCALING Scaling;
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        // DXGI_ALPHA_MODE AlphaMode;
        // UINT Flags;
    };
    ComPtr<IDXGISwapChain1> swapchain;
    if (FAILED(factory->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &swapchain)))
    {
        return nullptr;
    }

    return swapchain;
}

} // namespace wgut::dxgi
