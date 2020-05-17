#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_d2d1.h>
#include <wgut/wgut_shader.h>
#include <wgut/OrbitCamera.h>
#include <wgut/MeshBuilder.h>
#include <stdexcept>
#include <iostream>
#include <DirectXMath.h>

auto SHADER = R"(
struct VS
{
    float3 position: POSITION;
    float2 uv: TEXCOORD0;
};

struct PS
{
    float4 position: SV_POSITION;
    float2 uv: TEXCOORD0;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PS vsMain(VS input)
{
    PS output;
    output.position = mul(mul(Projection, View), float4(input.position, 1));
    output.uv = input.uv;
	return output;
}

float4 psMain(PS input): SV_TARGET
{
    return Texture.Sample(Sampler, input.uv);
    // return float4(1, 1, 1, 1);
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
    // create shader
    auto shader = wgut::d3d11::Shader::Create(device, vs.ByteCode, ps.ByteCode);

    // create cube
    auto builder = wgut::Cube::Create();

    auto vb = std::make_shared<wgut::d3d11::VertexBuffer>();
    vb->MeshData(device, vs.ByteCode, inputLayout->Elements(), builder);

    // drawable
    auto drawable = std::make_shared<wgut::d3d11::Drawable>(vb);
    auto &submesh = drawable->AddSubmesh();
    submesh->Count = vb->IndexCount();
    submesh->Shader = shader;

    return drawable;
}

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
std::pair<ComPtr<ID3D11Texture2D>, ComPtr<ID3D11ShaderResourceView>> CreateTexture(const ComPtr<ID3D11Device> &device)
{
    auto d2d = wgut::d2d1::DeviceFromD3D11(device);
    if (!d2d)
    {
        throw std::runtime_error("fail to create d2d");
    }

    auto texture = wgut::d3d11::CreateTexture(device, 512, 512);
    if (!texture)
    {
        throw std::runtime_error("fail to create texture");
    }

    auto bitmap = wgut::d2d1::BitmapFromD3D11(d2d, texture);
    if (!bitmap)
    {
        throw std::runtime_error("fail to create bitmap");
    }

    {
        d2d->SetTarget(bitmap.Get());
        d2d->BeginDraw();
        d2d->Clear(D2D1::ColorF(D2D1::ColorF::LightYellow));

        D2D1_SIZE_F size = d2d->GetSize();
        float rx = size.width / 2;
        float ry = size.height / 2;
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(rx, ry), rx, ry);
        ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(d2d->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), &brush)))
        {
            throw std::runtime_error("fail to create brush");
        }
        d2d->FillEllipse(ellipse, brush.Get());
        d2d->EndDraw();
    }

    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &srv)))
    {
        throw std::runtime_error("fail to create srv");
    }

    return std::make_pair(texture, srv);
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

    auto [texture, srv] = CreateTexture(device);
    ComPtr<ID3D11SamplerState> sampler;
    D3D11_SAMPLER_DESC samplerDesc{
        .Filter = D3D11_FILTER_ANISOTROPIC,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .MipLODBias = 0,
        .MaxAnisotropy = 16,
        .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
        .BorderColor = {0, 0, 0, 0},
        .MinLOD = 0,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    device->CreateSamplerState(&samplerDesc, &sampler);

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

        rt.UpdateViewport(device, state.Width, state.Height);
        rt.ClearAndSet(context, clearColor);

        ID3D11ShaderResourceView *srvs[] = {
            srv.Get(),
        };
        context->PSSetShaderResources(0, _countof(srvs), srvs);
        ID3D11SamplerState *samplers[] = {
            sampler.Get(),
        };
        context->PSSetSamplers(0, _countof(samplers), samplers);

        drawable->Draw(context, b0->Buffer());

        swapchain->Present(1, 0);

        // clear reference
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    // window closed

    return 0;
}
