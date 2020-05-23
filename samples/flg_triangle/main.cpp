#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_flg.h>
#include <stdexcept>
#include <iostream>

#ifndef WINDOW_NAME
#define WINDOW_NAME "WINDOW_NAME"
#endif

namespace wgut::shader::flg
{

auto SHADER = R"(
export float4 to_float4(float2 xy)
{
    return float4(xy, 0, 1);
}

export float4 white()
{
    return float4(1, 1, 1, 1);
}
)";

static std::pair<ComPtr<ID3DBlob>, std::string> VS(const ShaderLibrary &lib)
{
    Graph graph;
    auto input = graph.make_input(Param<float2>("POSITION"));
    auto to_float4 = graph.make_call<std::tuple<float2>, std::tuple<float4>>(lib.Module, "to_float4");
    auto output = graph.make_output(Param<float4>("SV_POSITION"));

    graph.passValue(input.src<0>(), to_float4.dst<0>());
    graph.passValue(to_float4.ret(), output.dst<0>());

    return Link(graph.flg, "vs_5_0", lib.Instance);
}

static std::pair<ComPtr<ID3DBlob>, std::string> PS(const ShaderLibrary &lib)
{
    Graph graph;
    auto white = graph.make_call<std::tuple<>, std::tuple<float4>>(lib.Module, "white");
    auto output = graph.make_output(Param<float4>("SV_TARGET"));

    graph.passValue(white.ret(), output.dst<0>());

    return Link(graph.flg, "ps_5_0", lib.Instance);
}

static wgut::shader::CompiledPtr FLG()
{
    auto [lib, moduleError] = CompileModule(SHADER);
    if (!lib.Instance)
    {
        std::cerr << moduleError << std::endl;
        throw std::runtime_error(moduleError);
    }
    auto [vs, vsError] = VS(lib);
    if (!vs)
    {
        std::cerr << vsError << std::endl;
        throw std::runtime_error(vsError);
    }
    auto [ps, psError] = PS(lib);
    if (!ps)
    {
        std::cerr << psError << std::endl;
        throw std::runtime_error(psError);
    }
    auto compiled = wgut::shader::Compiled::Create(vs, ps);
    if (!compiled)
    {
        std::cerr << "fail to inputLayout" << std::endl;
        throw std::runtime_error("fail to inputLayout");
    }
    return compiled;
}

} // namespace wgut::shader::flg

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

    // flg
    auto compiled = wgut::shader::flg::FLG();
    if (!compiled)
    {
        throw std::runtime_error("fail to compile shader");
    }
    // shader
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
