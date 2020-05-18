#include <wgut/Win32Window.h>
#include <wgut/wgut_d3d11.h>
#include <wgut/wgut_shader.h>
#include <stdexcept>
#include <iostream>

namespace DX
{

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // Set a breakpoint on this line to catch Win32 API errors.
        throw std::runtime_error("");
    }
}

} // namespace DX

auto SHADER = R"(
export float4 to_float4(float2 xy)
{
    return float4(xy, 0, 1);
}

export float4 white()
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

auto SAMPLE_SHADER = R"(
   // This file defines the defaults for the editor.

// This is the default code in the fixed header section.
// @@@ Begin Header
Texture2D<float3> Texture : register(t0);
SamplerState Anisotropic : register(s0);
cbuffer CameraData : register(b0)
{
    float4x4 Model;
    float4x4 View;
    float4x4 Projection;
};
cbuffer TimeVariantSignals : register(b1)
{
    float SineWave;
    float SquareWave;
    float TriangleWave;
    float SawtoothWave;
};
// @@@ End Header

// This is the default code in the source section.
// @@@ Begin Source
export void VertexFunction(
    inout float4 position, inout float2 texcoord, inout float4 normal)
{
    position = mul(position, Model);
    position = mul(position, View);
    position = mul(position, Projection);
    normal = mul(normal, Model);
}

export float3 ColorFunction(float2 texcoord, float3 normal)
{
    return Texture.Sample(Anisotropic, texcoord);
}
// @@@ End Source

// This code is not displayed, but is used as part of the linking process.
// @@@ Begin Hidden
cbuffer HiddenBuffer : register(b2)
{
    float3 LightDirection;
};

export float3 AddLighting(float3 color, float3 normal)
{
    static const float ambient = 0.2f;
    float brightness = ambient + (1.0f - ambient) * saturate(dot(normal, LightDirection));
    return color * brightness;
}

export float3 AddDepthFog(float3 color, float depth)
{
    float3 fogColor = float3(0.4f, 0.9f, 0.5f); // Greenish.
    return lerp(color, fogColor, exp2(-depth));
}

export float3 AddGrayscale(float3 color)
{
    float luminance = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    return float3(luminance, luminance, luminance);
}

export float4 Float3ToFloat4SetW1(float3 value)
{
    // Convert a float3 value to a float4 value with the w component set to 1.0f.
    // Used for initializing homogeneous 3D coordinates and generating fully opaque color values.
    return float4(value, 1.0f);
}
// @@@ End Hidden
 
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

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

static std::string ToStr(const ComPtr<ID3DBlob> &blob)
{
    std::string str;
    if (blob)
    {
        str.resize(blob->GetBufferSize());
        memcpy(str.data(), blob->GetBufferPointer(), str.size());
    }
    return str;
}

static std::string GetLastError(const ComPtr<ID3D11FunctionLinkingGraph> &flg)
{
    ComPtr<ID3DBlob> error;
    flg->GetLastError(&error);
    return ToStr(error);
}

void LinkingThrowIfFailed(HRESULT hr, ID3D11FunctionLinkingGraph *graph)
{
    if (FAILED(hr))
    {
        ComPtr<ID3DBlob> error;
        graph->GetLastError(&error);
        auto str = ToStr(error);
        throw std::runtime_error(str);
    }
}

static std::pair<ComPtr<ID3DBlob>, std::string> Link(
    const char *target,
    const ComPtr<ID3D11ModuleInstance> &flgInstance,
    const ComPtr<ID3D11ModuleInstance> &moduleInstance)
{
    // Create a linker and hook up the module instance.
    ComPtr<ID3D11Linker> linker;
    D3DCreateLinker(&linker);
    linker->UseLibrary(moduleInstance.Get());

    ComPtr<ID3DBlob> pBlob;
    ComPtr<ID3DBlob> errorBlob;
    if (FAILED(linker->Link(flgInstance.Get(), "main", target, 0, &pBlob, &errorBlob)))
    {
        auto str = ToStr(errorBlob);
        return {nullptr, str};
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
        auto str = ToStr(errorBlob);
        return {nullptr, str};
    }

    ComPtr<ID3D11Module> module;
    if (FAILED(D3DLoadModule(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), &module)))
    {
        return {nullptr, "fail to load module"};
    }

    return {module, ""};
}

static std::pair<ComPtr<ID3DBlob>, std::string> VS(const ComPtr<ID3D11Module> &module, const ComPtr<ID3D11ModuleInstance> &moduleInstance)
{
    // input
    D3D11_PARAMETER_DESC inParams[] = {
        {
            .Name = "input_position",
            .SemanticName = "POSITION",
            .Type = D3D_SVT_FLOAT,
            .Class = D3D_SVC_VECTOR,
            .Rows = 1,
            .Columns = 4,
            .InterpolationMode = D3D_INTERPOLATION_UNDEFINED,
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
    // auto [vsModule, vsError] = BuildGraph(inParams, outParams, module, "to_float4");
    ComPtr<ID3D11FunctionLinkingGraph> flg;
    if (FAILED(D3DCreateFunctionLinkingGraph(0, &flg)))
    {
        return {nullptr, "fail to create flg"};
    }

    ComPtr<ID3D11LinkingNode> input;
    if (FAILED(flg->SetInputSignature(
            inParams,
            static_cast<UINT>(_countof(inParams)),
            &input)))
    {
        return {nullptr, "fail to input signature"};
    }

    ComPtr<ID3D11LinkingNode> output;
    if (FAILED(flg->SetOutputSignature(
            outParams,
            static_cast<UINT>(_countof(outParams)),
            &output)))
    {
        return {nullptr, "fail to output signature"};
    }

    // in -> out
    if (FAILED(flg->PassValue(input.Get(), 0, output.Get(), 0)))
    {
        return {nullptr, GetLastError(flg)};
    }

    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> flgInstance;
    if (FAILED(flg->CreateModuleInstance(&flgInstance, nullptr)))
    {
        return {nullptr, "create module"};
    }

    // return {flgInstance, ""};

    // if (!vsModule)
    // {
    //     return {nullptr, vsError};
    // }

    auto [vs, linkError] = Link("vs_5_0", flgInstance, moduleInstance);
    if (!vs)
    {
        return {nullptr, linkError};
    }

    // ComPtr<ID3DBlob> vs;
    // if (FAILED(flg->GenerateHlsl(0, &vs)))
    // {
    //     return {nullptr, "generate"};
    // }

    return {vs, ""};
}

static std::pair<ComPtr<ID3DBlob>, std::string> PS(const ComPtr<ID3D11Module> &module, const ComPtr<ID3D11ModuleInstance> &moduleInstance)
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
            .InterpolationMode = D3D_INTERPOLATION_UNDEFINED,
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
                .Name = "output_color",
                .SemanticName = "SV_TARGET",
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

    ComPtr<ID3D11FunctionLinkingGraph> flg;
    if (FAILED(D3DCreateFunctionLinkingGraph(0, &flg)))
    {
        return {nullptr, "fail to create flg"};
    }

    // ComPtr<ID3D11LinkingNode> input;
    // if (FAILED(flg->SetInputSignature(
    //         inParams,
    //         static_cast<UINT>(_countof(inParams)),
    //         &input)))
    // {
    //     return {nullptr, "fail to input signature"};
    // }

    ComPtr<ID3D11LinkingNode> func;
    if (FAILED(flg->CallFunction("", module.Get(), "white", &func)))
    {
        auto str = GetLastError(flg);
        return {nullptr, str};
    }

    ComPtr<ID3D11LinkingNode> output;
    if (FAILED(flg->SetOutputSignature(
            outParams,
            static_cast<UINT>(_countof(outParams)),
            &output)))
    {
        return {nullptr, "fail to output signature"};
    }

    // func -> out
    if (FAILED(flg->PassValue(func.Get(), D3D_RETURN_PARAMETER_INDEX, output.Get(), 0)))
    {
        return {nullptr, "fail to func -> out"};
    }

    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> flgInstance;
    if (FAILED(flg->CreateModuleInstance(&flgInstance, nullptr)))
    {
        return {nullptr, "create module"};
    }

    auto [ps, linkError] = Link("ps_5_0", flgInstance, moduleInstance);
    if (!ps)
    {
        return {nullptr, linkError};
    }

    return {ps, ""};
}

static std::pair<wgut::shader::CompiledPtr, std::string> FLG(const ComPtr<ID3D11Module> &module)
{
    // Create an instance of the library and define resource bindings for it.
    // In this sample we use the same slots as the source library however this is not required.
    ComPtr<ID3D11ModuleInstance> moduleInstance;
    if (FAILED(module->CreateInstance("", &moduleInstance)))
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
        auto [vs, error] = VS(module, moduleInstance);
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
        auto [ps, error] = PS(module, moduleInstance);
        if (!ps)
        {
            return {nullptr, error};
        }
        compiled->PS = ps;
    }

    return {compiled, ""};
}

std::pair<wgut::shader::CompiledPtr, std::string> Sample(const std::string &source)
{
    ComPtr<ID3DBlob> codeBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(
        source.c_str(),
        source.size(),
        "ShaderModule",
        NULL,
        NULL,
        NULL,
        "lib_5_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &codeBlob,
        &errorBlob);

    // Note that even with successful compilation, this sample's library code will produce
    // warning messages as it targets level 9.1 but includes sampling instructions. The compiler
    // warns that these instructions are not available in the vertex shader stage for 9.1, indicating
    // that it is not valid to use those functions for function call nodes in a vertex shader graph.
    if (FAILED(hr))
    {
        std::string errorString;
        // OutputTextBox->Text = "Compilation failed.\n";
        if (errorBlob != nullptr)
        {
            errorString = std::string(static_cast<char *>(errorBlob->GetBufferPointer()));
            // errorString = CleanOutputMessage(errorString);
            // OutputTextBox->Text += ConvertString(errorString);
        }
        return {nullptr, errorString};
    }

    DX::ThrowIfFailed(codeBlob != nullptr ? S_OK : E_UNEXPECTED);

    // Load the compiled library code into a module object.
    ComPtr<ID3D11Module> shaderLibrary;
    DX::ThrowIfFailed(D3DLoadModule(codeBlob->GetBufferPointer(), codeBlob->GetBufferSize(), &shaderLibrary));

    // Create an instance of the library and define resource bindings for it.
    // In this sample we use the same slots as the source library however this is not required.
    ComPtr<ID3D11ModuleInstance> shaderLibraryInstance;
    DX::ThrowIfFailed(shaderLibrary->CreateInstance("", &shaderLibraryInstance));
    // HRESULTs for Bind methods are intentionally ignored as compiler optimizations may eliminate the source
    // bindings. In these cases, the Bind operation will fail, but the final shader will function normally.
    // shaderLibraryInstance->BindResource(0, 0, 1);
    // shaderLibraryInstance->BindSampler(0, 0, 1);
    // shaderLibraryInstance->BindConstantBuffer(0, 0, 0);
    // shaderLibraryInstance->BindConstantBuffer(1, 1, 0);
    // shaderLibraryInstance->BindConstantBuffer(2, 2, 0);

    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;

    LARGE_INTEGER linkStartTime;
    QueryPerformanceCounter(&linkStartTime);
    // for (int iteration = 0; iteration < SampleSettings::Benchmark::Iterations; iteration++)
    {
        // Construct a linking graph to represent the vertex shader.
        ComPtr<ID3D11FunctionLinkingGraph> vertexShaderGraph;
        DX::ThrowIfFailed(D3DCreateFunctionLinkingGraph(0, &vertexShaderGraph));

        // Define the main input node which will be fed by the Input Assembler pipeline stage.
        static const D3D11_PARAMETER_DESC vertexShaderInputParameters[] =
            {
                {"inputPos", "POSITION0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4, D3D_INTERPOLATION_LINEAR, D3D_PF_IN, 0, 0, 0, 0},
                // {"inputTex", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 2, D3D_INTERPOLATION_LINEAR, D3D_PF_IN, 0, 0, 0, 0},
                // {"inputNorm", "NORMAL0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 3, D3D_INTERPOLATION_LINEAR, D3D_PF_IN, 0, 0, 0, 0}
            };
        ComPtr<ID3D11LinkingNode> vertexShaderInputNode;
        LinkingThrowIfFailed(vertexShaderGraph->SetInputSignature(vertexShaderInputParameters, ARRAYSIZE(vertexShaderInputParameters), &vertexShaderInputNode), vertexShaderGraph.Get());

        // // VertexFunction in the sample HLSL code takes float4 arguments, so use the Float3ToFloat4SetW1 helper function to homogenize the coordinates.
        // // As both the position and normal values must be converted, we need two distinct call nodes.
        // ComPtr<ID3D11LinkingNode> homogenizeCallNodeForPos;
        // LinkingThrowIfFailed(vertexShaderGraph->CallFunction("", shaderLibrary.Get(), "Float3ToFloat4SetW1", &homogenizeCallNodeForPos), vertexShaderGraph.Get());
        // ComPtr<ID3D11LinkingNode> homogenizeCallNodeForNorm;
        // LinkingThrowIfFailed(vertexShaderGraph->CallFunction("", shaderLibrary.Get(), "Float3ToFloat4SetW1", &homogenizeCallNodeForNorm), vertexShaderGraph.Get());

        // // Define the graph edges from the input node to the helper function nodes.
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexShaderInputNode.Get(), 0, homogenizeCallNodeForPos.Get(), 0), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexShaderInputNode.Get(), 2, homogenizeCallNodeForNorm.Get(), 0), vertexShaderGraph.Get());

        // // Create a node for the main VertexFunction call using the output of the helper functions.
        // ComPtr<ID3D11LinkingNode> vertexFunctionCallNode;
        // LinkingThrowIfFailed(vertexShaderGraph->CallFunction("", shaderLibrary.Get(), "VertexFunction", &vertexFunctionCallNode), vertexShaderGraph.Get());

        // // Define the graph edges from the input node and helper function nodes.
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(homogenizeCallNodeForPos.Get(), D3D_RETURN_PARAMETER_INDEX, vertexFunctionCallNode.Get(), 0), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexShaderInputNode.Get(), 1, vertexFunctionCallNode.Get(), 1), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(homogenizeCallNodeForNorm.Get(), D3D_RETURN_PARAMETER_INDEX, vertexFunctionCallNode.Get(), 2), vertexShaderGraph.Get());

        // Define the main output node which will feed the Pixel Shader pipeline stage.
        static const D3D11_PARAMETER_DESC vertexShaderOutputParameters[] =
            {
                // {"outputTex", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 2, D3D_INTERPOLATION_UNDEFINED, D3D_PF_OUT, 0, 0, 0, 0},
                // {"outputNorm", "NORMAL0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 3, D3D_INTERPOLATION_UNDEFINED, D3D_PF_OUT, 0, 0, 0, 0},
                {"outputPos", "SV_POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4, D3D_INTERPOLATION_UNDEFINED, D3D_PF_OUT, 0, 0, 0, 0}};
        ComPtr<ID3D11LinkingNode> vertexShaderOutputNode;
        LinkingThrowIfFailed(vertexShaderGraph->SetOutputSignature(vertexShaderOutputParameters, ARRAYSIZE(vertexShaderOutputParameters), &vertexShaderOutputNode), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexFunctionCallNode.Get(), 0, vertexShaderOutputNode.Get(), 2), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexFunctionCallNode.Get(), 1, vertexShaderOutputNode.Get(), 0), vertexShaderGraph.Get());
        // LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexFunctionCallNode.Get(), 2, vertexShaderOutputNode.Get(), 1), vertexShaderGraph.Get());

        LinkingThrowIfFailed(vertexShaderGraph->PassValue(vertexShaderInputNode.Get(), 0, vertexShaderOutputNode.Get(), 0), vertexShaderGraph.Get());

        // Finalize the vertex shader graph.
        ComPtr<ID3D11ModuleInstance> vertexShaderGraphInstance;
        LinkingThrowIfFailed(vertexShaderGraph->CreateModuleInstance(&vertexShaderGraphInstance, nullptr), vertexShaderGraph.Get());

        // Create a linker and hook up the module instance.
        ComPtr<ID3D11Linker> linker;
        DX::ThrowIfFailed(D3DCreateLinker(&linker));
        DX::ThrowIfFailed(linker->UseLibrary(shaderLibraryInstance.Get()));

        // Link the vertex shader.
        ComPtr<ID3DBlob> errorBlob;
        if (FAILED(linker->Link(vertexShaderGraphInstance.Get(), "main", "vs_5_0", 0, &vertexShaderBlob, &errorBlob)))
        {
            throw errorBlob;
        }

        DX::ThrowIfFailed(vertexShaderBlob != nullptr ? S_OK : E_UNEXPECTED);
    }

    {
        // Construct a linking graph to represent the pixel shader.
        ComPtr<ID3D11FunctionLinkingGraph> pixelShaderGraph;
        DX::ThrowIfFailed(D3DCreateFunctionLinkingGraph(0, &pixelShaderGraph));

        // Define the main input node which will be fed by the Input Assembler pipeline stage.
        static const D3D11_PARAMETER_DESC pixelShaderInputParameters[] =
            {
                // {"inputTex", "TEXCOORD0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 2, D3D_INTERPOLATION_UNDEFINED, D3D_PF_IN, 0, 0, 0, 0},
                // {"inputNorm", "NORMAL0", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 3, D3D_INTERPOLATION_UNDEFINED, D3D_PF_IN, 0, 0, 0, 0},
                {"inputPos", "SV_POSITION", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4, D3D_INTERPOLATION_UNDEFINED, D3D_PF_IN, 0, 0, 0, 0}};
        ComPtr<ID3D11LinkingNode> pixelShaderInputNode;
        LinkingThrowIfFailed(pixelShaderGraph->SetInputSignature(pixelShaderInputParameters, ARRAYSIZE(pixelShaderInputParameters), &pixelShaderInputNode), pixelShaderGraph.Get());

        // // Create a node for the main ColorFunction call and connect it to the pixel shader inputs.
        // ComPtr<ID3D11LinkingNode> colorValueNode;
        // LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "ColorFunction", &colorValueNode), pixelShaderGraph.Get());

        // // Define the graph edges from the input node.
        // LinkingThrowIfFailed(pixelShaderGraph->PassValue(pixelShaderInputNode.Get(), 0, colorValueNode.Get(), 0), pixelShaderGraph.Get());
        // LinkingThrowIfFailed(pixelShaderGraph->PassValue(pixelShaderInputNode.Get(), 1, colorValueNode.Get(), 1), pixelShaderGraph.Get());

        // // Optionally insert additional function calls based on the toggle switches.
        // // After each call, we will swap nodes to ensure that the return value on the colorValueNode
        // // variable always represents the output color for the shader call graph.
        // // if (enableLighting)
        // {
        //     ComPtr<ID3D11LinkingNode> tempNode;
        //     LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "AddLighting", &tempNode), pixelShaderGraph.Get());
        //     LinkingThrowIfFailed(pixelShaderGraph->PassValue(colorValueNode.Get(), D3D_RETURN_PARAMETER_INDEX, tempNode.Get(), 0), pixelShaderGraph.Get());
        //     LinkingThrowIfFailed(pixelShaderGraph->PassValue(pixelShaderInputNode.Get(), 1, tempNode.Get(), 1), pixelShaderGraph.Get());
        //     colorValueNode.Swap(tempNode);
        // }

        // // if (enableDepthFog)
        // {
        //     ComPtr<ID3D11LinkingNode> tempNode;
        //     LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "AddDepthFog", &tempNode), pixelShaderGraph.Get());
        //     LinkingThrowIfFailed(pixelShaderGraph->PassValue(colorValueNode.Get(), D3D_RETURN_PARAMETER_INDEX, tempNode.Get(), 0), pixelShaderGraph.Get());
        //     LinkingThrowIfFailed(pixelShaderGraph->PassValueWithSwizzle(pixelShaderInputNode.Get(), 2, "z", tempNode.Get(), 1, "x"), pixelShaderGraph.Get());
        //     colorValueNode.Swap(tempNode);
        // }

        // // if (enableGrayscale)
        // {
        //     ComPtr<ID3D11LinkingNode> tempNode;
        //     LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "AddGrayscale", &tempNode), pixelShaderGraph.Get());
        //     LinkingThrowIfFailed(pixelShaderGraph->PassValue(colorValueNode.Get(), D3D_RETURN_PARAMETER_INDEX, tempNode.Get(), 0), pixelShaderGraph.Get());
        //     colorValueNode.Swap(tempNode);
        // }

        // // ColorFunction in the sample HLSL code returns a float3 argument, so use the Float3ToFloat4SetW1 helper function to set alpha (w) to 1.
        // ComPtr<ID3D11LinkingNode> fillAlphaCallNode;
        // LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "Float3ToFloat4SetW1", &fillAlphaCallNode), pixelShaderGraph.Get());

        // // Define the graph edges from the color node to the helper function node.
        // LinkingThrowIfFailed(pixelShaderGraph->PassValue(colorValueNode.Get(), D3D_RETURN_PARAMETER_INDEX, fillAlphaCallNode.Get(), 0), pixelShaderGraph.Get());

        ComPtr<ID3D11LinkingNode> white;
        LinkingThrowIfFailed(pixelShaderGraph->CallFunction("", shaderLibrary.Get(), "white", &white), pixelShaderGraph.Get());

        // Define the main output node which will feed the Output Merger pipeline stage.
        D3D11_PARAMETER_DESC pixelShaderOutputParameters[] =
            {
                {"outputColor", "SV_TARGET", D3D_SVT_FLOAT, D3D_SVC_VECTOR, 1, 4, D3D_INTERPOLATION_UNDEFINED, D3D_PF_OUT, 0, 0, 0, 0}};
        ComPtr<ID3D11LinkingNode> pixelShaderOutputNode;
        LinkingThrowIfFailed(pixelShaderGraph->SetOutputSignature(pixelShaderOutputParameters, ARRAYSIZE(pixelShaderOutputParameters), &pixelShaderOutputNode), pixelShaderGraph.Get());
        LinkingThrowIfFailed(pixelShaderGraph->PassValue(white.Get(), D3D_RETURN_PARAMETER_INDEX, pixelShaderOutputNode.Get(), 0), pixelShaderGraph.Get());

        // Finalize the pixel shader graph.
        ComPtr<ID3D11ModuleInstance> pixelShaderGraphInstance;
        LinkingThrowIfFailed(pixelShaderGraph->CreateModuleInstance(&pixelShaderGraphInstance, nullptr), pixelShaderGraph.Get());

        // Create a linker and hook up the module instance.
        ComPtr<ID3D11Linker> linker;
        DX::ThrowIfFailed(D3DCreateLinker(&linker));
        DX::ThrowIfFailed(linker->UseLibrary(shaderLibraryInstance.Get()));

        // Link the pixel shader.
        ComPtr<ID3DBlob> errorBlob;
        if (FAILED(linker->Link(pixelShaderGraphInstance.Get(), "main", "ps_5_0", 0, &pixelShaderBlob, &errorBlob)))
        {
            throw errorBlob;
        }

        DX::ThrowIfFailed(pixelShaderBlob != nullptr ? S_OK : E_UNEXPECTED);
    }

    auto compiled = std::make_shared<wgut::shader::Compiled>();
    compiled->VS = vertexShaderBlob;
    compiled->InputLayout = wgut::shader::InputLayout::Create(compiled->VS);
    compiled->PS = pixelShaderBlob;
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

    // module
    auto [module, moduleError] = CompileModule(SHADER);
    if (!module)
    {
        std::cerr << moduleError << std::endl;
        throw std::runtime_error(moduleError);
    }

    // flg
    auto [compiled, error] = Sample(SHADER);
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
