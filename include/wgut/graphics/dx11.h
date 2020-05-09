#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi1_3.h>
#include <vector>

namespace wgut
{

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace dxgi
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

ComPtr<IDXGISwapChain1> CreateSwapChain(ComPtr<ID3D11Device> device, HWND hwnd)
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

} // namespace dxgi

namespace d3d11
{

inline ComPtr<ID3D11Device> CreateDeviceForHardwareAdapter()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
    };
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels), D3D11_SDK_VERSION, &device, &level, nullptr)))
    {
        return nullptr;
    }

    return device;
}

} // namespace d3d11

} // namespace wgut
