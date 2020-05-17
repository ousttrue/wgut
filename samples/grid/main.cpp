#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <stdexcept>
#include <iostream>

// each View
// https://gamedev.stackexchange.com/questions/105572/c-struct-doesnt-align-correctly-to-a-pixel-shader-cbuffer
#pragma pack(push)
#pragma pack(16)
struct ViewConstants
{
    std::array<float, 16> b0View;
    std::array<float, 16> b0Projection;
    std::array<float, 3> b0CameraPosition;
    float _padding2;
    std::array<float, 2> b0ScreenSize;
    float fovY;
    float _padding3;
};
#pragma pack(pop)
static_assert(sizeof(ViewConstants) == 16 * 10, "sizeof ViewConstantsSize");

auto SHADER = R"(
struct VS_INPUT
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    linear float4 position : SV_POSITION;
    // linear float3 world : POSITION;
    linear float3 ray : RAY;
    linear float2 uv : TEXCOORD0;
};

struct PS_OUT
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 b0View : CAMERA_VIEW;
    float4x4 b0Projection : CAMERA_PROJECTION;
    float3 b0CameraPosition : CAMERA_POSITION;
    float2 b0ScreenSize : RENDERTARGET_SIZE;
    float fovY : CAMERA_FOVY;
};

PS_INPUT VSMain(VS_INPUT vs)
{
    PS_INPUT ps;

    ps.position = float4(vs.position, 1, 1);
    // ps.position = float4(vs.position, 0, 1);
    float halfFov = fovY / 2;
    float t = tan(halfFov);
    float aspectRatio = b0ScreenSize.x / b0ScreenSize.y;
    ps.ray = mul(float4(vs.position.x * t * aspectRatio, vs.position.y * t, -1, 0), b0View).xyz;

    ps.uv = vs.uv;
    return ps;
}

// https://gamedev.stackexchange.com/questions/141916/antialiasing-shader-grid-lines
float gridX(float x)
{
    float wrapped = frac(x + 0.5) - 0.5;
    float range = abs(wrapped);
    return range;
}

// https://stackoverflow.com/questions/50659949/hlsl-modify-depth-in-pixel-shader
PS_OUT PSMain(PS_INPUT ps)
{
    float3 n = b0CameraPosition.y >= 0 ? float3(0, -1, 0) : float3(0, 1, 0);

    float d = dot(n, ps.ray);
    clip(d);

    float t = dot(-b0CameraPosition, n) / d;
    float3 world = b0CameraPosition + t * ps.ray;

    float3 forward = float3(b0View[2][0], b0View[2][1], b0View[2][2]);
    float fn = 0.2 + smoothstep(0, 0.8, abs(dot(forward, n)));
    float near = 30 * fn;
    float far = near * 3;

    float distance = length(b0CameraPosition - world);
    float fade = smoothstep(1, 0, (distance - near) / (far - near));

    int modX = trunc(abs(world.x) + 0.5) % 5;
    int modY = trunc(abs(world.z) + 0.5) % 5;
    float thicknessX = modX ? 0.005 : 0.02;
    float thicknessY = modY ? 0.005 : 0.02;

    float2 dd = fwidth(world.xz);
    float gx = gridX(world.x);
    float gy = gridX(world.z);
    float lx = 1 - saturate(gx - thicknessX) / dd.x;
    float ly = 1 - saturate(gy - thicknessY) / dd.y;
    float c = max(lx, ly);
    c *= fade;

    float4 projected = mul(b0Projection, mul(b0View, float4(world, 1)));

    PS_OUT output = (PS_OUT)0;
    output.color = float4(float3(0.8, 0.8, 0.8) * c, 0.5);
    output.depth = projected.z/projected.w;

    return output;
}
)";

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
        SHADER, "VSMain",
        SHADER, "PSMain");
    if (!compiled)
    {
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }
    auto shader = wgut::d3d11::Shader::Create(device, compiled->VS, compiled->PS);

    // quad
    auto grid = wgut::d3d11::VertexBuffer::Create();
    grid->MeshData(device, compiled->VS, compiled->InputLayout->Elements(), wgut::mesh::Quad::Create());

    // camera
    auto camera = std::make_shared<wgut::OrbitCamera>(wgut::PerspectiveTypes::D3D);
    auto b0 = wgut::d3d11::ConstantBuffer<ViewConstants>::Create(device);

    // main loop
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        // update
        rt.UpdateViewport(device, state.Width, state.Height);

        {
            // update camera and constant buffer
            camera->Update(state);
            auto &cb = *b0->Payload();
            cb.b0ScreenSize = {(float)state.Width, (float)state.Height};
            cb.b0Projection = camera->state.projection;
            cb.b0View = camera->state.view;
            cb.b0CameraPosition = camera->state.position;
            cb.fovY = camera->state.fovYRadians;
            // cb.b0LightColor = m_light->LightColor;
            // cb.b0LightDir = m_light->LightDirection;
            b0->Upload(context);
        }

        // draw(D3D11DeviceContext)
        rt.ClearAndSet(context, clearColor);
        {
            // grid
            ID3D11Buffer *constants[] = {b0->Ptr()};
            shader->Setup(context, constants);
            grid->Draw(context);
        }

        // flush, present
        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
