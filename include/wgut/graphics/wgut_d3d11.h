#pragma once
#include "../ScreenState.h"
#include "wgut_common.h"
#include "wgut_dxgi.h"

namespace wgut::d3d11
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

class SwapChainRenderTarget
{
    ComPtr<IDXGISwapChain1> m_swapchain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    int m_backbufferWidth = 0;
    int m_backbufferHeight = 0;

public:
    SwapChainRenderTarget(const ComPtr<IDXGISwapChain1> swapchain)
        : m_swapchain(swapchain)
    {
    }

    bool Update(const ComPtr<ID3D11Device> &device, const wgut::ScreenState &state)
    {
        // size check
        if (m_rtv)
        {
            if (m_backbufferWidth != state.Width || m_backbufferHeight != state.Height)
            {
                // release RTV
                m_rtv.Reset();

                // resize
                DXGI_SWAP_CHAIN_DESC desc;
                m_swapchain->GetDesc(&desc);
                m_swapchain->ResizeBuffers(desc.BufferCount, state.Width, state.Height, desc.BufferDesc.Format, desc.Flags);

                m_backbufferWidth = state.Width;
                m_backbufferHeight = state.Height;
            }
        }

        // create RTV if not exists
        if (!m_rtv)
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
            if (FAILED(m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))))
            {
                return false;
            }

            if (FAILED(device->CreateRenderTargetView(backbuffer.Get(), nullptr, &m_rtv)))
            {
                return false;
            }
        }

        return true;
    }

    void ClearAndSet(const ComPtr<ID3D11DeviceContext> &context, const float clearColor[4])
    {
        // clear backbuffer
        context->ClearRenderTargetView(m_rtv.Get(), clearColor);

        ID3D11RenderTargetView *rtvs[] = {
            m_rtv.Get()};
        context->OMSetRenderTargets(_countof(rtvs), rtvs, nullptr);
    }
};

} // namespace wgut::d3d11
