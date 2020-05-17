#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <wgut/wgut_gizmo.h>
#include <stdexcept>
#include <iostream>
#include <DirectXMath.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct GizmoConstantBuffer
{
    std::array<float, 16> model;
    std::array<float, 16> viewProjection;
    std::array<float, 3> eye;
    float padding;
};
static_assert(sizeof(GizmoConstantBuffer) == sizeof(float) * (16 * 2 + 4));

auto SHADER = R"(
struct VS
{
    float3 position: POSITION;
};

struct PS
{
    float4 position: SV_POSITION;
};

cbuffer cbContextData : register(b0)
{
    float4x4 uModel;
    float4x4 uViewProj;
    float3 uEye;
};

PS vsMain(VS input)
{
    PS output;
    output.position = mul(uViewProj, mul(uModel, float4(input.position, 1)));
	return output;
}

float4 psMain(PS input): SV_TARGET
{
    return float4(1, 1, 1, 1);
}
)";

constexpr const char gizmo_shader[] = R"(
    struct VS_INPUT
	{
		float3 position : POSITION;
        float3 normal   : NORMAL;
		float4 color    : COLOR0;
	};
    struct VS_OUTPUT
    {
        linear float4 position: SV_POSITION;
        linear float3 normal  : NORMAL;
        linear float4 color   : COLOR0;
        linear float3 world   : POSITION;
    };    
	cbuffer cbContextData : register(b0)
	{
		float4x4 uModel;
		float4x4 uViewProj;
        float3 uEye;
	};
    
    VS_OUTPUT vsMain(VS_INPUT _in) 
    {
        VS_OUTPUT ret;
        ret.world = _in.position;
        ret.position = mul(uViewProj, float4(ret.world, 1));
        ret.normal = _in.normal;
        ret.color = _in.color;
        return ret;
    }

	float4 psMain(VS_OUTPUT _in): SV_Target
    {
        float3 light = float3(1, 1, 1) * max(dot(_in.normal, normalize(uEye - _in.world)), 0.50) + 0.25;
        return _in.color * float4(light, 1);
    }
)";

static auto CreateCube(const Microsoft::WRL::ComPtr<ID3D11Device> &device, const wgut::shader::CompiledPtr &compiled)
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

static void Draw(const ComPtr<ID3D11DeviceContext> &context,
                 const wgut::d3d11::ShaderPtr &shader,
                 const ComPtr<ID3D11Buffer> &b0,
                 const wgut::d3d11::VertexBufferPtr &vb)
{
    shader->Setup(context);
    ID3D11Buffer *buffers[] = {
        b0.Get(),
        // b1.Get(),
    };
    context->VSSetConstantBuffers(0, _countof(buffers), buffers);
    context->CSSetConstantBuffers(0, _countof(buffers), buffers);
    vb->Draw(context);
}

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
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);

    // cube
    auto [cubeCompiled, cubeError] = wgut::shader::Compile(
        SHADER, "vsMain",
        SHADER, "psMain");
    if (!cubeCompiled)
    {
        std::cerr << cubeError << std::endl;
        throw std::runtime_error(cubeError);
    }
    auto cubeShader = wgut::d3d11::Shader::Create(device, cubeCompiled->VS, cubeCompiled->PS);
    auto cubeVertexBuffer = CreateCube(device, cubeCompiled);
    falg::TRS cubeTransform = {};
    cubeTransform.translation = {-2, 0, 0};

    auto camera = std::make_shared<wgut::OrbitCamera>(wgut::PerspectiveTypes::D3D);
    struct SceneConstantBuffer
    {
        std::array<float, 16> View;
        std::array<float, 16> Projection;
    };

    auto b0 = wgut::d3d11::ConstantBuffer<GizmoConstantBuffer>::Create(device);

    // gizmo
    wgut::gizmo::GizmoSystem gizmo;
    enum class transform_mode
    {
        translate,
        rotate,
        scale
    };
    auto mode = transform_mode::translate;
    bool is_local = true;
    // gizmo mesh
    auto [gizmoCompiled, gizmoError] = wgut::shader::Compile(
        gizmo_shader, "vsMain",
        gizmo_shader, "psMain");
    if (!gizmoCompiled)
    {
        std::cerr << gizmoError << std::endl;
        throw std::runtime_error(gizmoError);
    }
    auto gizmoShader = wgut::d3d11::Shader::Create(device, gizmoCompiled->VS, gizmoCompiled->PS);
    auto gizmoVertexBuffer = std::make_shared<wgut::d3d11::VertexBuffer>();

    // main loop
    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::ScreenState state;
    std::bitset<128> lastState{};
    while (window.TryGetState(&state))
    {
        // update camera
        camera->Update(state);
        auto &cameraState = camera->state;
        auto viewProjection = cameraState.view * cameraState.projection;
        auto gizmoCB = b0->Payload();
        gizmoCB->viewProjection = viewProjection;
        gizmoCB->eye = camera->state.position;

        // gizmo new frame
        gizmo.begin(
            cameraState.position,
            cameraState.rotation,
            cameraState.ray_origin,
            cameraState.ray_direction,
            state.MouseLeftDown());

        if (state.KeyCode['R'])
        {
            mode = transform_mode::rotate;
        }
        if (state.KeyCode['T'])
        {
            mode = transform_mode::translate;
        }
        if (state.KeyCode['S'])
        {
            mode = transform_mode::scale;
        }
        if (!lastState['Z'] && state.KeyCode['Z'])
        {
            is_local = !is_local;
        }
        lastState = state.KeyCode;

        {
            //
            // manipulate and update gizmo
            //
            switch (mode)
            {
            case transform_mode::translate:
                wgut::gizmo::handle::translation(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                                 nullptr, cubeTransform.translation, cubeTransform.rotation);
                break;

            case transform_mode::rotate:
                wgut::gizmo::handle::rotation(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                              nullptr, cubeTransform.translation, cubeTransform.rotation);
                break;

            case transform_mode::scale:
                wgut::gizmo::handle::scale(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                           cubeTransform.translation, cubeTransform.rotation, cubeTransform.scale);
                break;
            }

            auto buffer = gizmo.end();
            gizmoVertexBuffer->Vertices(device, gizmoCompiled->VS, gizmoCompiled->InputLayout->Elements(), buffer.Vertices);
            gizmoVertexBuffer->Indices(device, buffer.Indices);
        }

        // update
        rt.UpdateViewport(device, state.Width, state.Height);
        b0->Payload()->model = cubeTransform.RowMatrix();
        b0->Upload(context);

        // draw
        rt.ClearAndSet(context, clearColor);
        Draw(context, cubeShader, b0->Buffer(), cubeVertexBuffer);
        Draw(context, gizmoShader, b0->Buffer(), gizmoVertexBuffer);

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
