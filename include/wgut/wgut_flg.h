#pragma once
#include "wgut_shader.h"

namespace wgut::shader::flg
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
    const ComPtr<ID3D11FunctionLinkingGraph> &flg,
    const char *target,
    const ComPtr<ID3D11ModuleInstance> &moduleInstance)
{
    // Finalize the vertex shader graph.
    ComPtr<ID3D11ModuleInstance> flgInstance;
    if (FAILED(flg->CreateModuleInstance(&flgInstance, nullptr)))
    {
        auto str = GetLastError(flg);
        return {nullptr, str};
    }

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

template <typename T>
struct Param
{
    using type = T;
    std::string name;

    Param(const std::string_view &_name)
        : name(_name)
    {
    }
};

struct float2
{
    float x;
    float y;
};
struct float3
{
    float x;
    float y;
    float z;
};
struct float4
{
    float x;
    float y;
    float z;
    float w;
};

std::tuple<> param_type()
{
    return std::tuple<>();
}

template <typename T, typename... TS>
decltype(auto) param_type(const T &t, const TS &... ts)
{
    auto rest = param_type(ts...);
    return std::tuple_cat(std::tuple<T::type>(), rest);
};

template <typename T>
struct Src
{
    ComPtr<ID3D11LinkingNode> node;
    int parameterIndex;
};
template <typename T>
struct Dst
{
    ComPtr<ID3D11LinkingNode> node;
    int parameterIndex;
};

template <typename IN_TYPES, typename OUT_TYPES>
struct Node
{
    ComPtr<ID3D11LinkingNode> node;

    Node(const ComPtr<ID3D11LinkingNode> &_node)
        : node(_node)
    {
    }

    template <UINT N>
    decltype(auto) src()
    {
        return Src<std::tuple_element<N, OUT_TYPES>::type>{node, N};
    }

    decltype(auto) ret()
    {
        return Src<std::tuple_element<0, OUT_TYPES>::type>{node, D3D_RETURN_PARAMETER_INDEX};
    }

    template <UINT N>
    decltype(auto) dst()
    {
        return Dst<std::tuple_element<N, IN_TYPES>::type>{node, N};
    }
};

template <std::size_t... Ns, typename... Ts>
auto tail_impl(std::index_sequence<Ns...>, std::tuple<Ts...> t)
{
    return std::make_tuple(std::get<Ns + 1u>(t)...);
}

template <typename... Ts>
auto tail(std::tuple<Ts...> t)
{
    return tail_impl(std::make_index_sequence<sizeof...(Ts) - 1u>(), t);
}

void add_input(std::vector<D3D11_PARAMETER_DESC> *list)
{
}

template <typename... TS>
void _add_vector_input(std::vector<D3D11_PARAMETER_DESC> *list, const char *name, UINT columns, const TS &... ts)
{
    list->push_back({
        .Name = name,
        .SemanticName = name,
        .Type = D3D_SVT_FLOAT,
        .Class = D3D_SVC_VECTOR,
        .Rows = 1,
        .Columns = columns,
        .InterpolationMode = D3D_INTERPOLATION_UNDEFINED,
        .Flags = D3D_PF_IN,
        .FirstInRegister = 0,
        .FirstInComponent = 0,
        .FirstOutRegister = 0,
        .FirstOutComponent = 0,
    });

    add_input(list, ts...);
}

template <typename... TS>
void add_input(std::vector<D3D11_PARAMETER_DESC> *list, const Param<float4> &t, const TS &... ts)
{
    _add_vector_input(list, t.name.c_str(), 4, ts...);
}
template <typename... TS>
void add_input(std::vector<D3D11_PARAMETER_DESC> *list, const Param<float2> &t, const TS &... ts)
{
    _add_vector_input(list, t.name.c_str(), 2, ts...);
}

void add_output(std::vector<D3D11_PARAMETER_DESC> *list)
{
}

template <typename... TS>
void add_output(std::vector<D3D11_PARAMETER_DESC> *list, const Param<float4> &t, const TS &... ts)
{
    list->push_back({
        .Name = t.name.c_str(),
        .SemanticName = t.name.c_str(),
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
    });

    add_output(list, ts...);
}

struct Graph
{
    ComPtr<ID3D11FunctionLinkingGraph> flg;

    Graph()
    {
        if (FAILED(D3DCreateFunctionLinkingGraph(0, &flg)))
        {
            throw "fail to create flg";
        }
    }

    template <typename... IN_PARAMS>
    decltype(auto) make_input(const IN_PARAMS &... inParams)
    {
        ComPtr<ID3D11LinkingNode> node;
        {
            std::vector<D3D11_PARAMETER_DESC> params;
            add_input(&params, inParams...);
            if (FAILED(flg->SetInputSignature(
                    params.data(),
                    static_cast<UINT>(params.size()),
                    &node)))
            {
                throw "fail to input signature";
            }
        }

        using inTypes = decltype(param_type(inParams...));
        return Node<std::tuple<>, inTypes>(node);
    }

    template <typename... OUT_PARAMS>
    decltype(auto) make_output(const OUT_PARAMS &... outParams)
    {
        ComPtr<ID3D11LinkingNode> node;
        {
            std::vector<D3D11_PARAMETER_DESC> params;
            add_output(&params, outParams...);
            if (FAILED(flg->SetOutputSignature(
                    params.data(),
                    static_cast<UINT>(params.size()),
                    &node)))
            {
                throw "fail to output signature";
            }
        }

        using outTypes = decltype(param_type(outParams...));
        return Node<outTypes, std::tuple<>>(node);
    }

    template <typename IN_PARAMS, typename OUT_PARAMS>
    decltype(auto) make_call(const ComPtr<ID3D11Module> &module, const char *name)
    {
        ComPtr<ID3D11LinkingNode> node;
        if (FAILED(flg->CallFunction("", module.Get(), name, &node)))
        {
            auto str = wgut::shader::flg::GetLastError(flg);
            throw str;
        }
        return Node<IN_PARAMS, OUT_PARAMS>(node);
    }

    template <typename T>
    void passValue(const Src<T> &src, const Dst<T> &dst)
    {
        if (FAILED(flg->PassValue(src.node.Get(), src.parameterIndex, dst.node.Get(), dst.parameterIndex)))
        {
            auto str = GetLastError(flg);
            throw str;
        }
    }
};

} // namespace wgut::shader::flg
