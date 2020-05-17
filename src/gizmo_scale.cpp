#include <wgut/wgut_gizmo.h>
#include "impl.h"
#include <iostream>

namespace wgut::gizmo
{

static void flush_to_zero(falg::float3 &f)
{
    if (std::abs(f[0]) < 0.02f)
        f[0] = 0.f;
    if (std::abs(f[1]) < 0.02f)
        f[1] = 0.f;
    if (std::abs(f[2]) < 0.02f)
        f[2] = 0.f;
}

template <typename T>
T clamp(const T &val, const T &min, const T &max) { return std::min(std::max(val, min), max); }

static bool dragger(const GizmoComponent &component,
                    const falg::Ray &worldRay, const GizmoState &state, bool uniform,
                    falg::float3 *out)
{
    auto plane_tangent = falg::Cross(
        component.axis,
        state.original.translation - worldRay.origin);
    auto N = falg::Cross(component.axis, plane_tangent);

    // If an intersection exists between the ray and the plane, place the object at that point
    auto t = worldRay >> falg::Plane{
                             N,
                             state.original.translation + state.offset};
    if (t < 0)
    {
        return false;
    }
    auto intersect = worldRay.SetT(t);
    auto offset_on_axis = falg::EachMul((intersect - state.offset - state.original.translation), component.axis);
    flush_to_zero(offset_on_axis);
    auto new_scale = state.original.scale + falg::size_cast<falg::float3>(offset_on_axis);

    if (uniform)
    {
        auto s = clamp(falg::Dot(intersect, new_scale), 0.01f, 1000.f);
        *out = falg::float3{s, s, s};
    }
    else
    {
        *out = falg::float3{
            clamp(new_scale[0], 0.01f, 1000.f),
            clamp(new_scale[1], 0.01f, 1000.f),
            clamp(new_scale[2], 0.01f, 1000.f)};
    }

    return true;
}

static falg::float2 mace_points[] = {{0.25f, 0}, {0.25f, 0.05f}, {1, 0.05f}, {1, 0.1f}, {1.25f, 0.1f}, {1.25f, 0}};

static GizmoComponent xComponent{
    geometry_mesh::make_lathed_geometry({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 16, mace_points, _countof(mace_points)),
    {1, 0.5f, 0.5f, 1.f},
    {1, 0, 0, 1.f},
    {1, 0, 0}};
static GizmoComponent yComponent{
    geometry_mesh::make_lathed_geometry({0, 1, 0}, {0, 0, 1}, {1, 0, 0}, 16, mace_points, _countof(mace_points)),
    {0.5f, 1, 0.5f, 1.f},
    {0, 1, 0, 1.f},
    {0, 1, 0}};
static GizmoComponent zComponent{
    geometry_mesh::make_lathed_geometry({0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 16, mace_points, _countof(mace_points)),
    {0.5f, 0.5f, 1, 1.f},
    {0, 0, 1, 1.f},
    {0, 0, 1}};

static const GizmoComponent *g_meshes[] = {&xComponent, &yComponent, &zComponent};

static std::pair<const GizmoComponent *, float> raycast(const falg::Ray &ray)
{
    const GizmoComponent *updated_state = nullptr;
    float best_t = std::numeric_limits<float>::infinity();
    for (auto mesh : g_meshes)
    {
        auto t = ray >> mesh->mesh;
        if (t < best_t)
        {
            updated_state = mesh;
            best_t = t;
        }
    }
    return std::make_pair(updated_state, best_t);
}

static void draw(const falg::Transform &t, std::vector<gizmo_renderable> &drawlist, const GizmoComponent *activeMesh)
{
    for (auto mesh : g_meshes)
    {
        gizmo_renderable r{
            .mesh = mesh->mesh,
            .color = (mesh == activeMesh) ? mesh->base_color : mesh->highlight_color,
        };
        for (auto &v : r.mesh.vertices)
        {
            // transform local coordinates into worldspace
            v.position = t.ApplyPosition(v.position);
            v.normal = t.ApplyDirection(v.normal);
        }
        drawlist.push_back(r);
    }
}

namespace handle
{

bool scale(const GizmoSystem &ctx, uint32_t id, bool is_uniform,
           const falg::float3 &t, const falg::float4 &r, falg::float3 &s)
{
    auto &impl = ctx.m_impl;
    auto [gizmo, created] = impl->get_or_create_gizmo(id);

    auto worldRay = falg::Ray{impl->state.ray_origin, impl->state.ray_direction};
    auto localRay = worldRay.Transform(falg::Transform{t, r}.Inverse());

    if (impl->state.has_clicked)
    {
        auto [updated_state, best_t] = raycast(localRay);

        if (updated_state)
        {
            auto localHit = localRay.SetT(best_t);
            auto offset = falg::Transform{t, r}.ApplyPosition(localHit) - t;
            gizmo->begin(updated_state, offset, {t, r, s}, {});
        }
    }
    else if (impl->state.has_released)
    {
        gizmo->end();
    }

    auto active = gizmo->active();
    if (active)
    {
        if (dragger(*active, localRay, gizmo->m_state, is_uniform, &s))
        {
        }
    }

    draw({t, r}, impl->drawlist, active);

    return gizmo->isHoverOrActive();
}

} // namespace handle
} // namespace wgut::gizmo
