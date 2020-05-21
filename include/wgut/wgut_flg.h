#pragma once
#include "wgut_shader.h"

namespace wgut::shader
{

inline std::string GetLastError(const ComPtr<ID3D11FunctionLinkingGraph> &flg)
{
    ComPtr<ID3DBlob> error;
    flg->GetLastError(&error);
    return ToStr(error);
}

struct ShaderLibrary
{
    // keep blob to link time
    ComPtr<ID3DBlob> Blob;
    ComPtr<ID3D11Module> Module;
    ComPtr<ID3D11ModuleInstance> Instance;
};

inline std::pair<ShaderLibrary, std::string> CompileModule(const std::string_view &shader)
{
    ShaderLibrary lib;
    ComPtr<ID3DBlob> error;
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
            &lib.Blob,
            &error)))
    {
        auto str = ToStr(error);
        return {{}, str};
    }

    if (FAILED(D3DLoadModule(lib.Blob->GetBufferPointer(), lib.Blob->GetBufferSize(), &lib.Module)))
    {
        return {{}, "fail to load module"};
    }

    // Create an instance of the library and define resource bindings for it.
    // In this sample we use the same slots as the source library however this is not required.
    if (FAILED(lib.Module->CreateInstance("", &lib.Instance)))
    {
        return {{}, "fail to create instance"};
    }

    return {lib, ""};
}

inline std::pair<ComPtr<ID3DBlob>, std::string> Link(
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

} // namespace wgut::shader
