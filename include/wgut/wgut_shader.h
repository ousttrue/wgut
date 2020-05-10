#pragma once
#include "wgut_common.h"
#include <d3dcompiler.h>
#include <gsl/span>
#include <memory>
#include <string_view>

namespace wgut::shader
{

struct Compiled
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

inline Compiled Compile(const std::string_view &source, const char *name,
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

    Compiled compiled;
    D3DCompile(source.data(), source.size(), name,
               define, include,
               entryPoint, target, compileFlags, 0, &compiled.ByteCode, &compiled.Error);
    return compiled;
}

inline Compiled CompileVS(const std::string_view &source, const char *entryPoint)
{
    return Compile(source, "vs", entryPoint, "vs_5_0", nullptr, nullptr);
}

inline Compiled CompilePS(const std::string_view &source, const char *entryPoint)
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
class InputLayout
{
    // keep semantic strings
    std::vector<std::string> m_semantics;
    std::vector<InputLayoutElement> m_layout;

public:
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
        layout->m_semantics.reserve(desc.InputParameters);
        for (unsigned i = 0; i < desc.InputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC lParamDesc;
            pReflection->GetInputParameterDesc(i, &lParamDesc);

            layout->m_semantics.push_back(lParamDesc.SemanticName);
            InputLayoutElement lElementDesc{
                .SemanticName = layout->m_semantics.back().c_str(),
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
};

} // namespace wgut::shader
