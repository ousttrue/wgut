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

struct float4
{
    float x;
    float y;
    float z;
    float w;
};

std::tuple<> param_type(const std::tuple<> &_)
{
    return std::tuple<>();
}

template <typename T, typename... TS>
decltype(auto) param_type(const std::tuple<T, TS...> &t)
{
    auto rest = param_type(std::tuple<TS...>());
    return std::tuple_cat(std::tuple<T::type>(), rest);
};

template <typename T>
struct Src
{
    ComPtr<ID3D11LinkingNode> node;
    UINT parameterIndex;
};
template <typename T>
struct Dst
{
    ComPtr<ID3D11LinkingNode> node;
    UINT parameterIndex;
};

template <typename IN_TYPES, typename OUT_TYPES>
struct Node
{
    ComPtr<ID3D11LinkingNode> node;

    template <UINT N>
    decltype(auto) src()
    {
        return Src<std::tuple_element<N, OUT_TYPES>::type>{node, N};
    }

    template <UINT N>
    decltype(auto) dst()
    {
        return Dst<std::tuple_element<N, IN_TYPES>::type>{node, N};
    }
};

template <typename IN_TYPES, typename OUT_TYPES>
struct Graph
{
    ComPtr<ID3D11FunctionLinkingGraph> flg;
    Node<std::tuple<>, IN_TYPES> input;
    Node<OUT_TYPES, std::tuple<>> output;

    template <typename T>
    void passValue(const Src<T> &src, const Dst<T> &dst)
    {
        flg->PassValue(src.node.Get(), src.parameterIndex, dst.node.Get(), dst.parameterIndex);
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

void add_input(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<> &_)
{
}

template <typename T, typename... TS>
void add_input(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<T, TS...> &ts);

template <typename... TS>
void add_input(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<Param<float4>, TS...> &ts)
{
    auto &t = std::get<0>(ts);
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

    add_input(list, tail(ts));
}

void add_output(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<> &_)
{
}

template <typename T, typename... TS>
void add_output(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<T, TS...> &ts);

template <typename... TS>
void add_output(std::vector<D3D11_PARAMETER_DESC> *list, const std::tuple<Param<float4>, TS...> &ts)
{
    auto &t = std::get<0>(ts);
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

    add_output(list, tail(ts));
}

template <typename IN_PARAMS, typename OUT_PARAMS>
decltype(auto) make_graph(const ComPtr<ID3D11FunctionLinkingGraph> &flg, const IN_PARAMS &inParams, const OUT_PARAMS &outParams)
{
    using inTypes = decltype(param_type(inParams));
    using outTypes = decltype(param_type(outParams));
    Graph<inTypes, outTypes> graph;
    if (flg)
    {
        graph.flg = flg;
    }
    else if (FAILED(D3DCreateFunctionLinkingGraph(0, &graph.flg)))
    {
        throw "fail to create flg";
    }

    if (std::tuple_size<IN_PARAMS>())
    {
        std::vector<D3D11_PARAMETER_DESC> params;
        add_input(&params, inParams);
        if (FAILED(graph.flg->SetInputSignature(
                params.data(),
                static_cast<UINT>(params.size()),
                &graph.input.node)))
        {
            throw "fail to input signature";
        }
    }

    {
        std::vector<D3D11_PARAMETER_DESC> params;
        add_output(&params, outParams);
        if (FAILED(graph.flg->SetOutputSignature(
                params.data(),
                static_cast<UINT>(params.size()),
                &graph.output.node)))
        {
            throw "fail to output signature";
        }
    }

    return graph;
}

} // namespace wgut::shader::flg
