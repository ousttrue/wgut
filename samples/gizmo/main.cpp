#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <wgut/wgut_gizmo.h>
#include <stdexcept>
#include <iostream>
#include <DirectXMath.h>

auto SHADER = R"(
struct VS
{
    float3 position: POSITION;
};

struct PS
{
    float4 position: SV_POSITION;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
};

PS vsMain(VS input)
{
    PS output;
    output.position = mul(mul(Projection, View), float4(input.position, 1));
	return output;
}

float4 psMain(PS input): SV_TARGET
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
    vb->Vertices(device, vs.ByteCode, inputLayout->Elements(), vertices);
    vb->Indices(device, indices);

    // create shader
    auto shader = wgut::d3d11::Shader::Create(device, vs.ByteCode, ps.ByteCode);

    auto drawable = std::make_shared<wgut::d3d11::Drawable>(vb);
    auto &submesh = drawable->AddSubmesh();
    submesh->Count = vb->IndexCount();
    submesh->Shader = shader;

    return drawable;
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

    auto drawable = CreateDrawable(device);

    auto camera = std::make_shared<wgut::OrbitCamera>(wgut::PerspectiveTypes::D3D);
    struct SceneConstantBuffer
    {
        std::array<float, 16> View;
        std::array<float, 16> Projection;
    };

    auto b0 = wgut::d3d11::ConstantBuffer<SceneConstantBuffer>::Create(device);

    // teapot a
    falg::TRS teapot_a;
    teapot_a.translation = {-2, 0, 0};


    wgut::gizmo::GizmoSystem gizmo;
    enum class transform_mode
    {
        translate,
        rotate,
        scale
    };
    auto mode = transform_mode::scale;
    bool is_local = true;

    float clearColor[4] = {0.3f, 0.2f, 0.1f, 1.0f};
    wgut::d3d11::SwapChainRenderTarget rt(swapchain);
    wgut::ScreenState state;
    while (window.TryGetState(&state))
    {
        {
            // update camera
            camera->Update(state);
            auto sceneData = b0->Data();
            sceneData->Projection = camera->state.projection;
            sceneData->View = camera->state.view;
            b0->Upload(context);
        }

        // gizmo new frame
        auto &cameraState = camera->state;
        gizmo.begin(
            cameraState.position,
            cameraState.rotation,
            cameraState.ray_origin,
            cameraState.ray_direction,
            state.MouseLeftDown());

        auto viewProjection = cameraState.view * cameraState.projection;

        {
            //
            // manipulate and update gizmo
            //
            switch (mode)
            {
            case transform_mode::translate:
                wgut::gizmo::handle::translation(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                             nullptr, teapot_a.translation, teapot_a.rotation);
                break;

            case transform_mode::rotate:
                wgut::gizmo::handle::rotation(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                          nullptr, teapot_a.translation, teapot_a.rotation);
                break;

            case transform_mode::scale:
                wgut::gizmo::handle::scale(gizmo, wgut::gizmo::hash_fnv1a("first-example-gizmo"), is_local,
                                       teapot_a.translation, teapot_a.rotation, teapot_a.scale);
                break;
            }

            auto buffer = gizmo.end();
            // gizmo_mesh->uploadMesh(device,
            //                        buffer.pVertices, buffer.verticesBytes, buffer.vertexStride,
            //                        buffer.pIndices, buffer.indicesBytes, buffer.indexStride,
            //                        true);
        }

        rt.UpdateViewport(device, state.Width, state.Height);
        rt.ClearAndSet(context, clearColor);

        drawable->Draw(context, b0->Buffer());

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
