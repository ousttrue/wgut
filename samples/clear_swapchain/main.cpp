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

    wgut::d3d11::SwapChainRenderTarget swrt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        swrt.Update(device, state);
        swrt.Begin(context, clearColor);
        swrt.End(context);
    }

    // window closed

    return 0;
}
