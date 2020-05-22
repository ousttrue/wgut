#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_flg.h>
#include <stdexcept>
#include <iostream>

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

static wgut::d3d11::VertexBufferPtr CreateVertexBuffer(
    const Microsoft::WRL::ComPtr<ID3D11Device> &device,
    const std::shared_ptr<wgut::shader::Compiled> &compiled)
{
    // create vertex buffer
    struct float4
    {
        float x;
        float y;
        float z;
        float w;
    };
    // clockwise
    float4 vertices[] = {
        {0.5f, -0.5f, 0, 1},
        {-0.5f, -0.5f, 0, 1},
        {0.0f, 0.5f, 0, 1}};
    uint16_t indices[] = {
        0, 1, 2};
    auto vb = std::make_shared<wgut::d3d11::VertexBuffer>();
    vb->Vertices(device, compiled->VS, compiled->InputLayout->Elements(), vertices);
    vb->Indices(device, indices);
    return vb;
}

namespace wgut::shader::flg
{

static std::pair<ComPtr<ID3DBlob>, std::string> VS(const ShaderLibrary &lib)
{
    auto graph = wgut::shader::flg::Graph();
    auto input = graph.make_input(std::make_tuple(Param<float4>("POSITION")));
    auto output = graph.make_output(std::make_tuple(Param<float4>("SV_POSITION")));

    graph.passValue(input.src<0>(), output.dst<0>());

    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> flgInstance;
    if (FAILED(graph.flg->CreateModuleInstance(&flgInstance, nullptr)))
    {
        return {nullptr, "create module"};
    }

    auto [vs, linkError] = Link("vs_5_0", flgInstance, lib.Instance);
    if (!vs)
    {
        return {nullptr, linkError};
    }

    return {vs, ""};
}

static std::pair<ComPtr<ID3DBlob>, std::string> PS(const wgut::shader::flg::ShaderLibrary &lib)
{
    wgut::shader::flg::Graph graph;

    // function node before set output signature
    ComPtr<ID3D11LinkingNode> func;
    if (FAILED(graph.flg->CallFunction("", lib.Module.Get(), "white", &func)))
    {
        auto str = wgut::shader::flg::GetLastError(graph.flg);
        return {nullptr, str};
    }

    auto output = graph.make_output(std::make_tuple(Param<float4>("SV_TARGET")));

    // func -> out
    if (FAILED(graph.flg->PassValue(func.Get(), D3D_RETURN_PARAMETER_INDEX, output.node.Get(), 0)))
    {
        return {nullptr, "fail to func -> out"};
    }

    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> flgInstance;
    if (FAILED(graph.flg->CreateModuleInstance(&flgInstance, nullptr)))
    {
        return {nullptr, "create module"};
    }

    auto [ps, linkError] = wgut::shader::flg::Link("ps_5_0", flgInstance, lib.Instance);
    if (!ps)
    {
        return {nullptr, linkError};
    }

    return {ps, ""};
}

} // namespace wgut::shader::flg

static std::pair<wgut::shader::CompiledPtr, std::string> FLG(const wgut::shader::flg::ShaderLibrary &lib)
{
    auto compiled = std::make_shared<wgut::shader::Compiled>();

    {
        auto [vs, error] = VS(lib);
        if (!vs)
        {
            return {nullptr, error};
        }
        compiled->VS = vs;

        auto inputLayout = wgut::shader::InputLayout::Create(vs);
        if (!inputLayout)
        {
            return {nullptr, "fail to inputlayout"};
        }
        compiled->InputLayout = inputLayout;
    }

    {
        auto [ps, error] = PS(lib);
        if (!ps)
        {
            return {nullptr, error};
        }
        compiled->PS = ps;
    }

    return {compiled, ""};
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
    auto [lib, moduleError] = wgut::shader::flg::CompileModule(SHADER);
    if (!lib.Instance)
    {
        std::cerr << moduleError << std::endl;
        throw std::runtime_error(moduleError);
    }

    auto [compiled, error] = FLG(lib);
    if (!compiled)
    {
        std::cerr << error << std::endl;
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
