#include <wgut/Win32Window.h>
#include <wgut/graphics/dx11.h>
#include <stdexcept>

int main(int argc, char **argv)
{
    wgut::Win32Window window(L"CLASS_NAME");
    auto hwnd = window.Create(L"WINDOW_NAME");
    if (!hwnd)
    {
        throw std::runtime_error("fail to create window");
    }
    window.Show();

    auto device = wgut::d3d11::CreateDeviceForHardwareAdapter();
    if (!device)
    {
        throw std::runtime_error("fail to create device");
    }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    auto swapchain = wgut::dxgi::CreateSwapChain(device, hwnd);
    if (!swapchain)
    {
        throw std::runtime_error("fail to create swapchain");
    }

    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};

    wgut::ScreenState state;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    int backbufferWidth = 0;
    int backbufferHeight = 0;
    while (window.TryGetState(&state))
    {
        // size check
        if (rtv)
        {
            if (backbufferWidth != state.Width || backbufferHeight != state.Height)
            {
                // release RTV
                rtv.Reset();

                // resize
                DXGI_SWAP_CHAIN_DESC desc;
                swapchain->GetDesc(&desc);
                swapchain->ResizeBuffers(desc.BufferCount, state.Width, state.Height, desc.BufferDesc.Format, desc.Flags);

                backbufferWidth = state.Width;
                backbufferHeight = state.Height;
            }
        }

        // create RTV if not exists
        if (!rtv)
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
            if (FAILED(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))))
            {
                throw std::runtime_error("fail to GetBuffer");
            }

            if (FAILED(device->CreateRenderTargetView(backbuffer.Get(), nullptr, &rtv)))
            {
                throw std::runtime_error("fail to create RTV");
            }
        }

        // clear backbuffer
        context->ClearRenderTargetView(rtv.Get(), clearColor);

        // apply
        swapchain->Present(1, 0);
    }

    // window closed

    return 0;
}
