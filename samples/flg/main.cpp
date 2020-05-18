#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <stdexcept>
#include <iostream>

auto SHADER = R"(
export float4 to_float4(float2 xy)
{
    return float4(xy, 0, 1);
}

export float4 white(float4 _)
{
    return float4(1, 1, 1, 1);
}

// // SV: system value
// float4 vsMain(float2 position: POSITION): SV_POSITION
// {
// 	return float4(position, 0, 1);
// }

// float4 psMain(float4 position: SV_POSITION): SV_TARGET
// {
//     return float4(1, 1, 1, 1);
// }
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

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

static std::pair<ComPtr<ID3DBlob>, std::string> Compile(
    const std::string_view &target,
    const gsl::span<const D3D11_PARAMETER_DESC> &inParams,
    const gsl::span<const D3D11_PARAMETER_DESC> &outParams,
    const ComPtr<ID3D11Module> &module,
    const ComPtr<ID3D11ModuleInstance> &shaderLibraryInstance,
    const std::string_view &funcName)
{
    ComPtr<ID3D11FunctionLinkingGraph> flg;
    if (FAILED(D3DCreateFunctionLinkingGraph(0, &flg)))
    {
        return {nullptr, "fail to create flg"};
    }

    ComPtr<ID3D11LinkingNode> input;
    if (FAILED(flg->SetInputSignature(
            inParams.data(),
            static_cast<UINT>(inParams.size()),
            &input)))
    {
        return {nullptr, "fail to input signature"};
    }

    ComPtr<ID3D11LinkingNode> func;
    if (FAILED(flg->CallFunction("", module.Get(), funcName.data(), &func)))
    {
        return {nullptr, "fail to callFunction"};
    }

    ComPtr<ID3D11LinkingNode> output;
    if (FAILED(flg->SetOutputSignature(
            outParams.data(),
            static_cast<UINT>(outParams.size()),
            &output)))
    {
        return {nullptr, "fail to output signature"};
    }

    // in -> func
    if (FAILED(flg->PassValue(input.Get(), 0, func.Get(), 0)))
    {
        return {nullptr, "fail to in -> func"};
    }

    // func -> out
    if (FAILED(flg->PassValue(func.Get(), D3D_RETURN_PARAMETER_INDEX, output.Get(), 0)))
    {
        return {nullptr, "fail to func -> out"};
    }

    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> shaderGraphInstance;
    if (FAILED(flg->CreateModuleInstance(&shaderGraphInstance, nullptr)))
    {
        return {nullptr, "create module"};
    }

    // Create a linker and hook up the module instance.
    ComPtr<ID3D11Linker> linker;
    D3DCreateLinker(&linker);
    linker->UseLibrary(shaderLibraryInstance.Get());

    // // Link the vertex shader.
    // ComPtr<ID3DBlob> errorBlob;
    // if (FAILED(linker->Link(vertexShaderGraphInstance.Get(), "main", ("vs" + m_shaderModelSuffix).c_str(), 0, &vertexShaderBlob, &errorBlob)))
    // {
    //     throw errorBlob;
    // }

    ComPtr<ID3DBlob> pBlob;
    // if (FAILED(flg->GenerateHlsl(0, &pBlob)))
    // {
    //     return {nullptr, "fail to output signature"};
    // }

    // Link the vertex shader.
    ComPtr<ID3DBlob> errorBlob;
    if (FAILED(linker->Link(shaderGraphInstance.Get(), "main", target.data(), 0, &pBlob, &errorBlob)))
    {
        return {nullptr, "link"};
    }

    return {pBlob, ""};
}

static std::pair<ComPtr<ID3D11Module>, std::string> CompileModule(const std::string_view &shader)
{
    ComPtr<ID3DBlob> codeBlob;
    ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3DCompile(
            shader.data(),
            static_cast<UINT>(shader.size()),
            "ShaderModule",
            NULL,
            NULL,
            NULL,
            "lib_5_0",
            0, //D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            &codeBlob,
            &errorBlob)))
    {
        std::string str;
        if (errorBlob)
        {
            str.resize(errorBlob->GetBufferSize());
            memcpy(str.data(), errorBlob->GetBufferPointer(), str.size());
        }
        return {nullptr, str};
    }

    ComPtr<ID3D11Module> module;
    if (FAILED(D3DLoadModule(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), &module)))
    {
        return {nullptr, "fail to load module"};
    }

    return {module, ""};
}

static std::pair<wgut::shader::CompiledPtr, std::string> FLG(const std::string_view &shader)
{
    // module
    auto [module, error] = CompileModule(shader);
    if (!module)
    {
        return {nullptr, error};
    }

    // Create an instance of the library and define resource bindings for it.
    // In this sample we use the same slots as the source library however this is not required.
    ComPtr<ID3D11ModuleInstance> shaderLibraryInstance;
    if (FAILED(module->CreateInstance("", &shaderLibraryInstance)))
    {
        return {nullptr, "create instance"};
    }

    // HRESULTs for Bind methods are intentionally ignored as compiler optimizations may eliminate the source
    // bindings. In these cases, the Bind operation will fail, but the final shader will function normally.
    // shaderLibraryInstance->BindResource(0, 0, 1);
    // shaderLibraryInstance->BindSampler(0, 0, 1);
    // shaderLibraryInstance->BindConstantBuffer(0, 0, 0);
    // shaderLibraryInstance->BindConstantBuffer(1, 1, 0);
    // shaderLibraryInstance->BindConstantBuffer(2, 2, 0);

    auto compiled = std::make_shared<wgut::shader::Compiled>();

    {
        // input
        D3D11_PARAMETER_DESC inParams[] = {
            {
                .Name = "input_position",
                .SemanticName = "POSITION",
                .Type = D3D_SVT_FLOAT,
                .Class = D3D_SVC_VECTOR,
                .Rows = 1,
                .Columns = 2,
                .InterpolationMode = D3D_INTERPOLATION_LINEAR,
                .Flags = D3D_PF_IN,
                .FirstInRegister = 0,
                .FirstInComponent = 0,
                .FirstOutRegister = 0,
                .FirstOutComponent = 0,
            },
        };

        // output
        D3D11_PARAMETER_DESC outParams[] =
            {
                {
                    .Name = "output_position",
                    .SemanticName = "SV_POSITION",
                    .Type = D3D_SVT_FLOAT,
                    .Class = D3D_SVC_VECTOR,
                    .Rows = 1,
                    .Columns = 4,
                    .InterpolationMode = D3D_INTERPOLATION_UNDEFINED,
                    .Flags = D3D_PF_OUT,
                    .FirstInRegister = 0,
                    .FirstInComponent = 0,
                    .FirstOutRegister = 0,
                    .FirstOutComponent = 0,
                },
            };
        auto [vs, vsError] = Compile("vs_5_0", inParams, outParams, module, shaderLibraryInstance, "to_float4");
        if (!vs)
        {
            return {nullptr, vsError};
        }
        compiled->VS = vs;
        auto inputLayout = wgut::shader::InputLayout::Create(vs);
        if (!inputLayout)
        {
            inputLayout = wgut::shader::InputLayout::Create(gsl::span<D3D11_PARAMETER_DESC>(inParams, 1));
        }
        compiled->InputLayout = inputLayout;
    }

    {
        // input
        D3D11_PARAMETER_DESC inParams[] = {
            {
                .Name = "input_position",
                .SemanticName = "SV_POSITION",
                .Type = D3D_SVT_FLOAT,
                .Class = D3D_SVC_VECTOR,
                .Rows = 1,
                .Columns = 4,
                .InterpolationMode = D3D_INTERPOLATION_LINEAR,
                .Flags = D3D_PF_IN,
                .FirstInRegister = 0,
                .FirstInComponent = 0,
                .FirstOutRegister = 0,
                .FirstOutComponent = 0,
            },
        };

        // output
        D3D11_PARAMETER_DESC outParams[] =
            {
                {
                    .Name = "output_position",
                    .SemanticName = "SV_POSITION",
                    .Type = D3D_SVT_FLOAT,
                    .Class = D3D_SVC_VECTOR,
                    .Rows = 1,
                    .Columns = 4,
                    .InterpolationMode = D3D_INTERPOLATION_UNDEFINED,
                    .Flags = D3D_PF_OUT,
                    .FirstInRegister = 0,
                    .FirstInComponent = 0,
                    .FirstOutRegister = 0,
                    .FirstOutComponent = 0,
                },
            };
        auto [ps, psError] = Compile("ps_5_0", inParams, outParams, module, shaderLibraryInstance, "white");
        if (!ps)
        {
            return {nullptr, psError};
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
    auto [compiled, error] = FLG(SHADER);
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
