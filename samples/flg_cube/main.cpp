#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <wgut/wgut_flg.h>
#include <stdexcept>
#include <iostream>
#include <DirectXMath.h>

#ifndef WINDOW_NAME
#define WINDOW_NAME "WINDOW_NAME"
#endif

namespace wgut::shader::flg
{

auto SHADER = R"(
cbuffer CameraData : register(b0)
{
    float4x4 View;
    float4x4 Projection;
};

export float4 mul_vp(float3 position)
{
    return mul(mul(Projection, View), float4(position, 1));
}

export float4 white()
{
    return float4(1, 1, 1, 1);
}
)";

static std::pair<ComPtr<ID3DBlob>, std::string> VS(const ShaderLibrary &lib)
{
    lib.Instance->BindConstantBuffer(0, 0, 0);

    Graph graph;
    auto input = graph.make_input(Param<float3>("POSITION"));
    auto mul_vp = graph.make_call<std::tuple<float3>, std::tuple<float4>>(lib.Module, "mul_vp");
    auto output = graph.make_output(Param<float4>("SV_POSITION"));

    graph.passValue(input.src<0>(), mul_vp.dst<0>());
    graph.passValue(mul_vp.ret(), output.dst<0>());

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

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

//
// should create from shader reflection
//
namespace shaderlib
{
template <typename T, size_t N>
struct Range
{
    static const size_t size_bytes = sizeof(T) * N;
    T *data;

    Range(void *p)
        : data((T *)p)
    {
    }
};

class ICBGetter
{
public:
    virtual void View(const Range<float, 16> &bytes) const = 0;
    virtual void Projection(const Range<float, 16> &bytes) const = 0;
};
void CBUpdate(const ComPtr<ID3D11DeviceContext> &context,
              const ComPtr<ID3D11Buffer> &cb,
              const ICBGetter *getter)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        throw "map";
    }

    {
        getter->View(Range<float, 16>(mapped.pData));
        getter->Projection(Range<float, 16>(((uint8_t *)mapped.pData + 64)));
    }

    context->Unmap(cb.Get(), 0);
}

} // namespace shaderlib

//
// implement
//
class CBGetter : public shaderlib::ICBGetter
{
    const wgut::OrbitCamera *m_camera;

public:
    CBGetter(const wgut::OrbitCamera *camera)
        : m_camera(camera)
    {
    }

    void View(const shaderlib::Range<float, 16> &bytes) const override
    {
        memcpy(bytes.data, m_camera->state.view.data(), bytes.size_bytes);
    }

    void Projection(const shaderlib::Range<float, 16> &bytes) const override
    {
        memcpy(bytes.data, m_camera->state.projection.data(), bytes.size_bytes);
    }
};

wgut::d3d11::VertexBufferPtr CreateVertexBuffer(const ComPtr<ID3D11Device> &device,
                                                const wgut::shader::CompiledPtr &compiled)
{
    // create vertex buffer
    struct float3
    {
        float x;
        float y;
        float z;
    };
    // counter clockwise ?
    float3 vertices[] = {
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    };
    uint16_t indices[] = {
        0, 1, 2, 2, 3, 0, // z-
        1, 5, 6, 6, 2, 1, // x+
        5, 4, 7, 7, 6, 5, // z+
        4, 0, 3, 3, 7, 4, // x-
        3, 2, 6, 6, 7, 3, // y+
        0, 4, 5, 5, 1, 0, // y-
    };
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

    auto swapchain = wgut::dxgi::CreateSwapChain(device, hwnd);
    if (!swapchain)
    {
        throw std::runtime_error("fail to create swapchain");
    }
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);

    // compile shader
    auto compiled = wgut::shader::flg::FLG();
    if (!compiled)
    {
        throw std::runtime_error("compile");
    }
    auto shader = wgut::d3d11::Shader::Create(device, compiled->VS, compiled->PS);
    auto vb = CreateVertexBuffer(device, compiled);

    // camera
    auto camera = std::make_shared<wgut::OrbitCamera>(wgut::PerspectiveTypes::D3D);
    CBGetter getter(camera.get());

    auto b0 = wgut::d3d11::CreateConstantBuffer(device, sizeof(float) * 32);

    // main loop
    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        // update camera
        camera->Update(state);

        // update
        rt.UpdateViewport(device, state.Width, state.Height);
        CBUpdate(context, b0, &getter);

        // draw
        rt.ClearAndSet(context, clearColor);
        ID3D11Buffer *constants[] = {b0.Get()};
        shader->Setup(context, constants);
        vb->Draw(context);

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
