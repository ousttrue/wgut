#pragma once
#include <string>
#include <array>
#include <stdint.h>
#include <span>

namespace wgut::gizmo
{

struct Vertex
{
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 4> color;
};

struct GizmoSystem
{
    struct gizmo_system_impl *m_impl = nullptr;

    GizmoSystem();
    ~GizmoSystem();

    // Clear geometry buffer and update internal `GizmoFrameState` data
    void begin(
        const std::array<float, 3> &camera_position,
        const std::array<float, 4> &camera_rotation,
        const std::array<float, 3> &ray_origin,
        const std::array<float, 3> &ray_direction,
        bool button);

    struct Buffer
    {
        // uint8_t *pVertices;
        // uint32_t verticesBytes;
        // uint32_t vertexStride;
        // uint8_t *pIndices;
        // uint32_t indicesBytes;
        // uint32_t indexStride;
        std::span<const Vertex> Vertices;
        std::span<const uint32_t> Indices;
    };
    Buffer end();
};

// 32 bit FNV Hash
uint32_t hash_fnv1a(const std::string &str);
} // namespace wgut::gizmo

#include "falg.h"

namespace wgut::gizmo::handle
{

bool translation(const GizmoSystem &system, uint32_t id, bool is_local,
                 const falg::Transform *parent, falg::float3 &t, const falg::float4 &r);
bool rotation(const GizmoSystem &system, uint32_t id, bool is_local,
              const falg::Transform *parent, const falg::float3 &t, falg::float4 &r);
bool scale(const GizmoSystem &system, uint32_t id, bool is_uniform,
           const falg::float3 &t, const falg::float4 &r, falg::float3 &s);

} // namespace wgut::gizmo::handle
