#include <wgut/Win32Window.h>
#include <wgut/graphics/wgut_d3d11.h>
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

    float vertices[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.0f, 0.5f};
    uint16_t indices[] = {
        0, 1, 2};

    auto shader = wgut::d3d11::CreateShader(vs, ps);
    auto mesh = wgut::d3d11::CreateMesh(device, vertices, indices);


    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        rt.Update(device, state);
        rt.ClearAndSet(context, clearColor);

        shader->Setup(context);
        mesh->Draw(context);

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
