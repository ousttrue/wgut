#pragma once
#include "ScreenState.h"
#include <array>
#include <DirectXMath.h>

namespace wgut
{

enum class PerspectiveTypes
{
    OpenGL,
    D3D,
};

struct CameraState
{
    // projection
    float fovYRadians = 60.0f / 180.0f * 3.14f;
    std::array<float, 16> projection;

    // view
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = 1;
    int viewportHeight = 1;
    std::array<float, 16> view;

    // viewInverse;
    std::array<float, 3> position;
    std::array<float, 4> rotation;

    // ray for mousecursor
    std::array<float, 3> ray_origin;
    std::array<float, 3> ray_direction;
};

using namespace DirectX;
struct Transform
{
    std::array<float, 3> translation = {};
    std::array<float, 4> rotation = {};

    Transform()
    {
    }

    Transform(const std::array<float, 3> &p, const std::array<float, 4> &r)
        : translation(p), rotation(r)
    {
    }

    Transform operator*(const Transform &rhs) const
    {
        auto t = XMVectorAdd(XMVector3Rotate(XMLoadFloat3((XMFLOAT3 *)&translation), XMLoadFloat4((XMFLOAT4 *)&rhs.rotation)), XMLoadFloat3((XMFLOAT3 *)&rhs.translation));
        auto r = XMQuaternionMultiply(XMLoadFloat4((XMFLOAT4 *)&rotation), XMLoadFloat4((XMFLOAT4 *)&rhs.rotation));

        Transform transform;
        XMStoreFloat3((XMFLOAT3 *)&transform.translation, t);
        XMStoreFloat4((XMFLOAT4 *)&transform.rotation, r);
        return transform;
    }

    std::array<float, 16> RowMatrix() const
    {
        std::array<float, 16> r;
        XMStoreFloat4x4((XMFLOAT4X4 *)&r, XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4 *)&rotation)));
        r[12] = translation[0];
        r[13] = translation[1];
        r[14] = translation[2];
        return r;
    }

    Transform Inverse() const
    {
        auto inv_r = XMQuaternionConjugate(XMLoadFloat4((XMFLOAT4 *)&rotation));
        auto inv_t = XMVector3Rotate(XMVectorScale(XMLoadFloat3((XMFLOAT3 *)&translation), -1), inv_r);

        Transform transform;
        XMStoreFloat3((XMFLOAT3 *)&transform.translation, inv_t);
        XMStoreFloat4((XMFLOAT4 *)&transform.rotation, inv_r);

        return transform;
    }

    std::array<float, 3> ApplyDirection(const std::array<float, 3> &dir) const
    {
        std::array<float, 3> result;
        XMStoreFloat3((XMFLOAT3 *)&result, XMVector3Rotate(XMLoadFloat3((XMFLOAT3 *)&dir), XMLoadFloat4((XMFLOAT4 *)&rotation)));
        return result;
    }
};

struct OrbitCamera
{
    CameraState state;

    PerspectiveTypes perspectiveType = PerspectiveTypes::D3D;

    float zNear = 0.1f;
    float zFar = 100.0f;
    float aspectRatio = 1.0f;

    int prevMouseX = -1;
    int prevMouseY = -1;

    std::array<float, 3> gaze{};
    std::array<float, 3> shift{0, 0.2f, -4};
    float yawRadians = 0;
    float pitchRadians = 0;

    OrbitCamera(PerspectiveTypes type = PerspectiveTypes::D3D)
        : perspectiveType(type)
    {
    }
    void CalcView(int w, int h, int x, int y)
    {
        // view transform
        auto origin = Transform(gaze, {0, 0, 0, 1});
        auto y_axis = XMFLOAT3{0.0f, 1.0f, 0.0f};
        auto q_yaw = XMQuaternionRotationAxis(XMLoadFloat3(&y_axis), yawRadians);
        auto x_axis = XMFLOAT3{1.0f, 0.0f, 0.0f};
        auto q_pitch = XMQuaternionRotationAxis(XMLoadFloat3(&x_axis), pitchRadians);
        std::array<float, 4> rot;
        XMStoreFloat4((XMFLOAT4 *)&rot, XMQuaternionMultiply(q_yaw, q_pitch));
        auto transform = origin * Transform{shift, rot};
        state.view = transform.RowMatrix();

        // inverse view transform
        auto inv = transform.Inverse();
        {
            state.rotation = inv.rotation;
            state.position = inv.translation;
        }

        // ray for mouse cursor
        auto t = std::tan(state.fovYRadians / 2);
        const float xx = 2 * (float)x / w - 1;
        const float yy = 1 - 2 * (float)y / h;
        auto dir = std::array<float, 3>{
            t * aspectRatio * xx,
            t * yy,
            -1,
        };
        state.ray_direction = inv.ApplyDirection(dir);
        state.ray_origin = state.position;
    }

    void CalcPerspective()
    {
        switch (perspectiveType)
        {
        case PerspectiveTypes::OpenGL:
            throw std::runtime_error("not implemented");
            break;

        case PerspectiveTypes::D3D:
            XMStoreFloat4x4((XMFLOAT4X4 *)&state.projection, XMMatrixPerspectiveFovRH(state.fovYRadians, aspectRatio, zNear, zFar));
            break;
        }
    }

    void SetViewport(int x, int y, int w, int h)
    {
        if (w == state.viewportWidth && h == state.viewportHeight)
        {
            return;
        }

        if (h == 0 || w == 0)
        {
            aspectRatio = 1.0f;
        }
        else
        {
            aspectRatio = w / (float)h;
        }
        state.viewportX = x;
        state.viewportY = y;
        state.viewportWidth = w;
        state.viewportHeight = h;
        CalcPerspective();

        return;
    }

    void Update(const ScreenState &window)
    {
        SetViewport(0, 0, window.Width, window.Height);

        if (prevMouseX != -1 && prevMouseY != -1)
        {
            auto deltaX = window.MouseX - prevMouseX;
            auto deltaY = window.MouseY - prevMouseY;

            if (window.Has(MouseButtonFlags::RightDown))
            {
                const auto FACTOR = 1.0f / 180.0f * 1.7f;
                yawRadians += deltaX * FACTOR;
                pitchRadians += deltaY * FACTOR;
            }
            if (window.Has(MouseButtonFlags::MiddleDown))
            {
                shift[0] -= deltaX / (float)state.viewportHeight * shift[2];
                shift[1] += deltaY / (float)state.viewportHeight * shift[2];
            }
            if (window.Has(MouseButtonFlags::WheelPlus))
            {
                shift[2] *= 0.9f;
            }
            else if (window.Has(MouseButtonFlags::WheelMinus))
            {
                shift[2] *= 1.1f;
            }
        }
        prevMouseX = window.MouseX;
        prevMouseY = window.MouseY;
        CalcView(window.Width, window.Height, window.MouseX, window.MouseY);
    }
};

} // namespace wgut