#pragma once
#include "wgut_common.h"
#include "wgut_dxgi.h"
#include "wgut_shader.h"
#include <gsl/span>

namespace wgut::d3d11
{

inline ComPtr<ID3D11Device> CreateDeviceForHardwareAdapter()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
    };
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, _countof(levels), D3D11_SDK_VERSION, &device, &level, nullptr)))
    {
        return nullptr;
    }

    return device;
}

class SwapChainRenderTarget
{
    ComPtr<IDXGISwapChain1> m_swapchain;
    ComPtr<ID3D11RenderTargetView> m_rtv;
    D3D11_VIEWPORT m_viewport = {};

public:
    SwapChainRenderTarget(const ComPtr<IDXGISwapChain1> swapchain)
        : m_swapchain(swapchain)
    {
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
    }

    bool UpdateViewport(const ComPtr<ID3D11Device> &device, int viewportWidth, int viewportHeight)
    {
        // size check
        if (m_rtv)
        {
            if (m_viewport.Width != viewportWidth || m_viewport.Height != viewportHeight)
            {
                // release RTV
                m_rtv.Reset();

                // resize
                DXGI_SWAP_CHAIN_DESC desc;
                m_swapchain->GetDesc(&desc);
                m_swapchain->ResizeBuffers(desc.BufferCount, viewportWidth, viewportHeight, desc.BufferDesc.Format, desc.Flags);

                m_viewport.Width = static_cast<float>(viewportWidth);
                m_viewport.Height = static_cast<float>(viewportHeight);
            }
        }

        // create RTV if not exists
        if (!m_rtv)
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
            if (FAILED(m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))))
            {
                return false;
            }

            if (FAILED(device->CreateRenderTargetView(backbuffer.Get(), nullptr, &m_rtv)))
            {
                return false;
            }
        }

        return true;
    }

    void ClearAndSet(const ComPtr<ID3D11DeviceContext> &context, const float clearColor[4])
    {
        // clear backbuffer
        context->ClearRenderTargetView(m_rtv.Get(), clearColor);

        ID3D11RenderTargetView *rtvs[] = {
            m_rtv.Get()};
        context->OMSetRenderTargets(_countof(rtvs), rtvs, nullptr);

        D3D11_VIEWPORT viewports[] =
            {
                m_viewport,
            };
        context->RSSetViewports(_countof(viewports), viewports);
    }
};

class VertexBuffer
{
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11Buffer> m_vertices;
    UINT m_stride = 0;
    ComPtr<ID3D11Buffer> m_indices;
    DXGI_FORMAT m_indexFormat = DXGI_FORMAT_UNKNOWN;
    UINT m_indexCount = 0;

public:
    template <typename T>
    bool Vertices(const ComPtr<ID3D11Device> &device,
                  const ComPtr<ID3DBlob> &vsByteCode, const gsl::span<const ::wgut::shader::InputLayoutElement> &layout,
                  const gsl::span<const T> &vertices)
    {
        if (FAILED(device->CreateInputLayout(
                (const D3D11_INPUT_ELEMENT_DESC *)layout.data(), static_cast<UINT>(layout.size()),
                vsByteCode->GetBufferPointer(), vsByteCode->GetBufferSize(),
                &m_layout)))
        {
            return false;
        }

        m_stride = sizeof(T);
        D3D11_BUFFER_DESC desc{
            .ByteWidth = static_cast<UINT>(vertices.size_bytes()),
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = 0,
        };
        D3D11_SUBRESOURCE_DATA data{
            .pSysMem = vertices.data(),
            .SysMemPitch = sizeof(T),
            .SysMemSlicePitch = static_cast<UINT>(vertices.size_bytes()),
        };
        if (FAILED(device->CreateBuffer(&desc, &data, &m_vertices)))
        {
            return false;
        }

        return true;
    }

    bool Indices(const ComPtr<ID3D11Device> &device, const void *p, UINT count, UINT stride)
    {
        m_indexCount = count;

        switch (stride)
        {
        case 2:
            m_indexFormat = DXGI_FORMAT_R16_UINT;
            break;

        case 4:
            m_indexFormat = DXGI_FORMAT_R32_UINT;
            break;

        default:
            return false;
        }

        UINT byteWidth = count * stride;
        D3D11_BUFFER_DESC desc{
            .ByteWidth = byteWidth,
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
            .StructureByteStride = 0,
        };
        D3D11_SUBRESOURCE_DATA data{
            .pSysMem = p,
            .SysMemPitch = stride,
            .SysMemSlicePitch = byteWidth,
        };
        if (FAILED(device->CreateBuffer(&desc, &data, &m_indices)))
        {
            return false;
        }

        return true;
    }

    bool Indices(const ComPtr<ID3D11Device> &device, const gsl::span<uint16_t> &span)
    {
        return Indices(device, span.data(), static_cast<UINT>(span.size()), 2);
    }

    void Indices(const ComPtr<ID3D11Device> &device, const gsl::span<uint32_t> &span)
    {
        throw std::runtime_error("not implemented");
    }

    void Draw(const ComPtr<ID3D11DeviceContext> &context)
    {
        Draw(context, m_indexCount, 0);
    }

    void Draw(const ComPtr<ID3D11DeviceContext> &context, UINT drawCount, UINT drawOffset)
    {
        context->IASetInputLayout(m_layout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer *buffers[] = {
            m_vertices.Get(),
        };
        UINT strides[] = {
            m_stride,
        };
        UINT offsets[] = {
            0,
        };
        context->IASetVertexBuffers(0, _countof(buffers), buffers, strides,
                                    offsets);
        context->IASetIndexBuffer(m_indices.Get(), m_indexFormat, 0);
        // context->DrawIndexedInstanced(drawCount, 1, drawOffset, 0, 0);
        context->DrawIndexed(drawCount, drawOffset, 0);
    }
};
using VertexBufferPtr = std::shared_ptr<VertexBuffer>;

class Shader
{
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;

public:
    static std::shared_ptr<Shader> Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3DBlob> &vs, const ComPtr<ID3DBlob> &ps)
    {
        auto shader = std::shared_ptr<Shader>(new Shader);

        if (FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &shader->m_vs)))
        {
            return nullptr;
        }
        if (FAILED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &shader->m_ps)))
        {
            return nullptr;
        }

        return shader;
    }

    void Setup(const ComPtr<ID3D11DeviceContext> &context)
    {
        context->VSSetShader(m_vs.Get(), nullptr, 0);
        context->PSSetShader(m_ps.Get(), nullptr, 0);
    }
};
using ShaderPtr = std::shared_ptr<Shader>;

class Drawable
{
    VertexBufferPtr m_mesh;
    ShaderPtr m_shader;

public:
    Drawable(const VertexBufferPtr &mesh, const ShaderPtr &shader)
        : m_mesh(mesh), m_shader(shader)
    {
    }

    void Draw(const ComPtr<ID3D11DeviceContext> &context)
    {
        m_shader->Setup(context);
        m_mesh->Draw(context);
    }
};
using DrawablePtr = std::shared_ptr<Drawable>;

} // namespace wgut::d3d11
