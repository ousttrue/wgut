#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <stdexcept>

int main(int argc, char **argv)
{
    wgut::Win32Window window(L"CLASS_NAME");
    auto hwnd = window.Create(WINDOW_NAME);
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

    wgut::d3d11::SwapChainRenderTarget rt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        rt.UpdateViewport(device, state.Width, state.Height);
        rt.ClearAndSet(context, clearColor);
        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
