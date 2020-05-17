#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <stdexcept>
#include <iostream>

auto SHADER = R"(
// SV: system value
float4 vsMain(float2 position: POSITION): SV_POSITION
{
	return float4(position, 0, 1);
}

float4 psMain(float4 position: SV_POSITION): SV_TARGET
{
    return float4(1, 1, 1, 1);
}
)";

static wgut::d3d11::VertexBufferPtr CreateVertexBuffer(
    const Microsoft::WRL::ComPtr<ID3D11Device> &device,
    const std::shared_ptr<wgut::shader::Compiled> &compiled)
{
    // create vertex buffer
    struct float2
    {
        float x;
        float y;
    };
    // clockwise
    float2 vertices[] = {
        {0.5f, -0.5f},
        {-0.5f, -0.5f},
        {0.0f, 0.5f}};
    uint16_t indices[] = {
        0, 1, 2};
    auto vb = std::make_shared<wgut::d3d11::VertexBuffer>();
    vb->Vertices(device, compiled->VS, compiled->InputLayout->Elements(), vertices);
    vb->Indices(device, indices);
    return vb;
}

int main(int argc, char **argv)
{
    // window
    wgut::Win32Window window(L"CLASS_NAME");
    auto hwnd = window.Create(WINDOW_NAME);
    if (!hwnd)
    {
        throw std::runtime_error("fail to create window");
    }
    window.Show();

    // device
    auto device = wgut::d3d11::CreateDeviceForHardwareAdapter();
    if (!device)
    {
        throw std::runtime_error("fail to create device");
    }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    // swapchain and render target
    auto swapchain = wgut::dxgi::CreateSwapChain(device, hwnd);
    if (!swapchain)
    {
        throw std::runtime_error("fail to create swapchain");
    }
    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);

    // shader
    auto [compiled, error] = wgut::shader::Compile(
        SHADER, "vsMain",
        SHADER, "psMain");
    if (!compiled)
    {
        throw std::runtime_error(error);
    }
    auto shader = wgut::d3d11::Shader::Create(device, compiled->VS, compiled->PS);

    // triangle
    auto triangle = CreateVertexBuffer(device, compiled);

    // main loop
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        // update
        rt.UpdateViewport(device, state.Width, state.Height);

        // draw(D3D11DeviceContext)
        rt.ClearAndSet(context, clearColor);
        shader->Setup(context);
        triangle->Draw(context);

        // flush, present
        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
