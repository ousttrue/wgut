#include <wgut/Win32Window.h>
#include <wgut/graphics/wgut_d3d11.h>
#include <wgut/graphics/wgut_shader.h>
#include <stdexcept>
#include <iostream>

auto SHADER = R"(
float4 vsMain(float2 position: POSITION): SV_POSITION
{
	return float4(position, 0, 1);
}

float4 psMain(float4 position: SV_POSITION): SV_TARGET
{
    return float4(1, 1, 1, 1);
}
)";

static wgut::d3d11::DrawablePtr CreateDrawable(const Microsoft::WRL::ComPtr<ID3D11Device> &device)
{
    // compile shader
    auto vs = wgut::shader::CompileVS(SHADER, "vsMain");
    if (!vs.ByteCode)
    {
        std::cerr << vs.error_str() << std::endl;
        throw std::runtime_error("fail to compile vs");
    }
    auto inputLayout = wgut::shader::InputLayout::Create(vs.ByteCode);
    if (!inputLayout)
    {
        throw std::runtime_error("fail to ReflectInputLayout");
    }
    auto ps = wgut::shader::CompilePS(SHADER, "psMain");
    if (!ps.ByteCode)
    {
        std::cerr << ps.error_str() << std::endl;
        throw std::runtime_error("fail to compile ps");
    }

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
    auto mesh = std::make_shared<wgut::d3d11::VertexBuffer>();
    mesh->Vertices<float2>(device, vs.ByteCode, inputLayout->Elements(), vertices);
    mesh->Indices(device, indices);

    // create shader
    auto shader = wgut::d3d11::Shader::Create(device, vs.ByteCode, ps.ByteCode);

    auto drawable = std::make_shared<wgut::d3d11::Drawable>(mesh, shader);

    return drawable;
}

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

    auto drawable = CreateDrawable(device);

    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        rt.UpdateViewport(device, state.Width, state.Height);
        rt.ClearAndSet(context, clearColor);

        drawable->Draw(context);

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}