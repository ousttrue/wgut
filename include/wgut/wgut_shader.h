#pragma once
#include "wgut_common.h"
#include <d3dcompiler.h>
#include <gsl/span>
#include <memory>
#include <string_view>
#include <stdexcept>

namespace wgut::shader
{

inline std::string ToStr(const ComPtr<ID3DBlob> &blob)
{
    std::string str;
    if (blob)
    {
        str.resize(blob->GetBufferSize());
        memcpy(str.data(), blob->GetBufferPointer(), str.size());
    }
    return str;
}

struct CompileResult
{
    ComPtr<ID3DBlob> ByteCode;
    ComPtr<ID3DBlob> Error;

    std::string error_str() const
    {
        std::string str;
        if (Error)
        {
            str.resize(Error->GetBufferSize());
            memcpy(str.data(), Error->GetBufferPointer(), str.size());
        }
        return str;
    }
};

inline CompileResult Compile(const std::string_view &source, const char *name,
                             const char *entryPoint, const char *target,
                             const D3D_SHADER_MACRO *define, ID3DInclude *include)
{
    // Create the pipeline state, which includes compiling and loading shaders.
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    CompileResult compiled;
    D3DCompile(source.data(), source.size(), name,
               define, include,
               entryPoint, target, compileFlags, 0, &compiled.ByteCode, &compiled.Error);
    return compiled;
}

inline CompileResult CompileVS(const std::string_view &source, const char *entryPoint)
{
    return Compile(source, "vs", entryPoint, "vs_5_0", nullptr, nullptr);
}

inline CompileResult CompilePS(const std::string_view &source, const char *entryPoint)
{
    return Compile(source, "ps", entryPoint, "ps_5_0", nullptr, nullptr);
}

enum INPUT_CLASSIFICATION
{
    INPUT_CLASSIFICATION_PER_VERTEX_DATA = 0,
    INPUT_CLASSIFICATION_PER_INSTANCE_DATA = 1
};
struct InputLayoutElement
{
    LPCSTR SemanticName;
    UINT SemanticIndex;
    DXGI_FORMAT Format;
    UINT InputSlot;
    UINT AlignedByteOffset;
    INPUT_CLASSIFICATION InputSlotClass;
    UINT InstanceDataStepRate;
};
inline UINT Stride(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32_FLOAT:
        return 8;

    case DXGI_FORMAT_R32G32B32_FLOAT:
        return 12;

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return 16;
    }

    throw std::runtime_error("unknown format");
}

class InputLayout
{
    std::vector<InputLayoutElement> m_layout;

public:
    static const char *POSITION;
    static const char *NORMAL;
    static const char *COLOR;
    static const char *TEXCOORD;

    static const char *GetSemanticConstant(const std::string_view semantic)
    {
        if (semantic == POSITION)
        {
            return POSITION;
        }
        if (semantic == NORMAL)
        {
            return NORMAL;
        }
        if (semantic == COLOR)
        {
            return COLOR;
        }
        if (semantic == TEXCOORD)
        {
            return TEXCOORD;
        }

        throw new std::runtime_error("not found");
        return nullptr;
    }

    static DXGI_FORMAT GetFormat(D3D_SHADER_VARIABLE_TYPE t, D3D_SHADER_VARIABLE_CLASS c, UINT rows, UINT cols)
    {
        if (t == D3D_SVT_FLOAT && c == D3D_SVC_VECTOR)
        {
            if (rows == 1)
            {
                switch (cols)
                {
                case 2:
                    return DXGI_FORMAT_R32G32_FLOAT;

                case 3:
                    return DXGI_FORMAT_R32G32B32_FLOAT;

                case 4:
                    return DXGI_FORMAT_R32G32B32A32_FLOAT;
                }
            }
        }

        return DXGI_FORMAT_UNKNOWN;
    }

    static std::shared_ptr<InputLayout> Create(const gsl::span<const D3D11_PARAMETER_DESC> &inParams)
    {
        auto layout = std::shared_ptr<InputLayout>(new InputLayout);

        for (auto &param : inParams)
        {
            auto semantic = GetSemanticConstant(param.SemanticName);
            InputLayoutElement element{
                .SemanticName = semantic,
                .SemanticIndex = 0,
                .InputSlot = 0,
                .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            };
            element.Format = GetFormat(param.Type, param.Class, param.Rows, param.Columns);
            layout->m_layout.push_back(element);
        }

        return layout;
    }

    static std::shared_ptr<InputLayout> Create(const ComPtr<ID3DBlob> vsByteCode)
    {
        ComPtr<ID3D11ShaderReflection> pReflection;
        if (FAILED(D3DReflect(vsByteCode->GetBufferPointer(), vsByteCode->GetBufferSize(), IID_PPV_ARGS(&pReflection))))
        {
            return nullptr;
        }

        auto layout = std::shared_ptr<InputLayout>(new InputLayout);

        // Define the vertex input layout.
        // D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        //     {
        //         {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        //         {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        //         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        //     };
        // auto inputElementDescs = InputLayoutFromReflection(vertexShader);

        D3D11_SHADER_DESC desc;
        pReflection->GetDesc(&desc);
        for (unsigned i = 0; i < desc.InputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC lParamDesc;
            pReflection->GetInputParameterDesc(i, &lParamDesc);

            auto semantic = GetSemanticConstant(lParamDesc.SemanticName);

            InputLayoutElement lElementDesc{
                .SemanticName = semantic,
                .SemanticIndex = lParamDesc.SemanticIndex,
                .InputSlot = 0,
                .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
                .InputSlotClass = INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            };

            if (lParamDesc.Mask == 1)
            {
                if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32_UINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32_SINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                    lElementDesc.Format = DXGI_FORMAT_R32_FLOAT;
            }
            else if (lParamDesc.Mask <= 3)
            {
                if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32_UINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32_SINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
            }
            else if (lParamDesc.Mask <= 7)
            {
                if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
            }
            else if (lParamDesc.Mask <= 15)
            {
                if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
                else if (lParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                    lElementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else
            {
                throw "unknown";
            }

            layout->m_layout.push_back(lElementDesc);
        }

        return layout;
    }

    gsl::span<const InputLayoutElement> Elements() const
    {
        return m_layout;
    }

    UINT Stride() const
    {
        UINT stride = 0;
        for (auto &element : m_layout)
        {
            stride += ::wgut::shader::Stride(element.Format);
        }
        return stride;
    }
};
const char *InputLayout::POSITION = "POSITION";
const char *InputLayout::NORMAL = "NORMAL";
const char *InputLayout::COLOR = "COLOR";
const char *InputLayout::TEXCOORD = "TEXCOORD";
using InputLayoutPtr = std::shared_ptr<InputLayout>;

struct Compiled
{
    ComPtr<ID3DBlob> VS;
    InputLayoutPtr InputLayout;
    ComPtr<ID3DBlob> PS;

    static std::shared_ptr<Compiled> Create(const ComPtr<ID3DBlob> &vs, const ComPtr<ID3DBlob> &ps)
    {
        auto compiled = std::make_shared<Compiled>();

        auto inputLayout = wgut::shader::InputLayout::Create(vs);
        if (!inputLayout)
        {
            return nullptr;
        }
        compiled->InputLayout = inputLayout;
        compiled->VS = vs;
        compiled->PS = ps;

        return compiled;
    }
};
using CompiledPtr = std::shared_ptr<Compiled>;

inline std::pair<std::shared_ptr<Compiled>, std::string> Compile(
    const std::string_view &vsSource, const char *vsEntryPoint,
    const std::string_view &psSource, const char *psEntryPoint)
{
    // compile shader
    auto vs = wgut::shader::CompileVS(vsSource, vsEntryPoint);
    if (!vs.ByteCode)
    {
        auto error = vs.error_str();
        return std::make_pair(nullptr, error);
    }
    auto inputLayout = wgut::shader::InputLayout::Create(vs.ByteCode);
    if (!inputLayout)
    {
        return std::make_pair(nullptr, "fail to ReflectInputLayout");
    }
    auto ps = wgut::shader::CompilePS(psSource, psEntryPoint);
    if (!ps.ByteCode)
    {
        auto error = ps.error_str();
        return std::make_pair(nullptr, error);
    }

    auto compiled = std::make_shared<Compiled>();
    compiled->VS = vs.ByteCode;
    compiled->InputLayout = inputLayout;
    compiled->PS = ps.ByteCode;
    return std::make_pair(compiled, "");
}

} // namespace wgut::shader
