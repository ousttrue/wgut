#pragma once
#include "wgut_common.h"
#include <d2d1_1.h>
#include <d3d11.h>

namespace wgut::d2d1
{

ComPtr<ID2D1DeviceContext> DeviceFromD3D11(const ComPtr<ID3D11Device> &device)
{
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device.As(&dxgiDevice)))
    {
        return false;
    }

    D2D1_FACTORY_OPTIONS options = {};
#if _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    ComPtr<ID2D1Factory1> factory;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 __uuidof(ID2D1Factory1), &options,
                                 &factory)))
    {
        return nullptr;
    }

    ComPtr<ID2D1Device> d2d;
    if (FAILED(factory->CreateDevice(dxgiDevice.Get(),
                                     &d2d)))
    {
        return nullptr;
    }

    ComPtr<ID2D1DeviceContext> d2dContext;
    D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
    if (FAILED(d2d->CreateDeviceContext(deviceOptions, &d2dContext)))
    {
        return nullptr;
    }

    return d2dContext;
}

ComPtr<ID2D1Bitmap1> BitmapFromD3D11(const ComPtr<ID2D1DeviceContext> &context, const ComPtr<ID3D11Texture2D> &texture)
{
    ComPtr<IDXGISurface> surface;
    if (FAILED(texture.As(&surface)))
    {
        return nullptr;
    }

    auto prop = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> bitmap;

    if (FAILED(context->CreateBitmapFromDxgiSurface(surface.Get(), &prop, &bitmap)))
    {
        return nullptr;
    }

    return bitmap;
}

} // namespace wgut::d2d1
