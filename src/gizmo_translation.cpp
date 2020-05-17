#include <wgut/wgut_gizmo.h>
#include "impl.h"

namespace wgut::gizmo
{

static bool planeDragger(const GizmoComponent &component, const falg::Ray &worldRay, const GizmoState &state,
                         falg::float3 *translation, const falg::float3 &N)
{

    falg::Plane plane(N, state.original.translation + state.offset);
    auto t = worldRay >> plane;
    if (t < 0)
    {
        return false;
    }

    auto intersect = worldRay.SetT(t);
    *translation = intersect - state.offset;

    return true;
}

static bool axisDragger(const GizmoComponent &component, const falg::Ray &worldRay, const GizmoState &state,
                        falg::float3 *translation, const falg::float3 &axis)
{
    // First apply a plane translation dragger with a plane that contains the desired axis and is oriented to face the camera
    auto plane_tangent = falg::Cross(axis, *translation - worldRay.origin);
    auto plane_normal = falg::Cross(axis, plane_tangent);
    if (!planeDragger(component, worldRay, state, translation, plane_normal))
    {
        return false;
    }

    // Constrain object motion to be along the desired axis
    *translation = state.original.translation + axis * falg::Dot(*translation - state.original.translation, axis);
    return true;
}

///
///      |\ 
/// +----+ \ 
/// |       \ 
///
static falg::float2 arrow_points[] = {{0.25f, 0}, {0.25f, 0.05f}, {1, 0.05f}, {1, 0.10f}, {1.2f, 0}};

static GizmoComponent componentX{
    .mesh = geometry_mesh::make_lathed_geometry({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 16, arrow_points, _countof(arrow_points)),
    .base_color = {1, 0.5f, 0.5f, 1.f},
    .highlight_color = {1, 0, 0, 1.f},
    .axis = {1, 0, 0}};
static GizmoComponent componentY{
    geometry_mesh::make_lathed_geometry({0, 1, 0}, {0, 0, 1}, {1, 0, 0}, 16, arrow_points, _countof(arrow_points)),
    {0.5f, 1, 0.5f, 1.f},
    {0, 1, 0, 1.f},
    {0, 1, 0}};
static GizmoComponent componentZ{
    geometry_mesh::make_lathed_geometry({0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 16, arrow_points, _countof(arrow_points)),
    {0.5f, 0.5f, 1, 1.f},
    {0, 0, 1, 1.f},
    {0, 0, 1}};
static GizmoComponent componentXY{
    geometry_mesh::make_box_geometry({0.25, 0.25, -0.01f}, {0.75f, 0.75f, 0.01f}),
    {1, 1, 0.5f, 0.5f},
    {1, 1, 0, 0.6f},
    {0, 0, 1}};
static GizmoComponent componentYZ{
    geometry_mesh::make_box_geometry({-0.01f, 0.25, 0.25}, {0.01f, 0.75f, 0.75f}),
    {0.5f, 1, 1, 0.5f},
    {0, 1, 1, 0.6f},
    {1, 0, 0}};
static GizmoComponent componentZX{
    geometry_mesh::make_box_geometry({0.25, -0.01f, 0.25}, {0.75f, 0.01f, 0.75f}),
    {1, 0.5f, 1, 0.5f},
    {1, 0, 1, 0.6f},
    {0, 1, 0}};
static GizmoComponent componentXYZ{
    geometry_mesh::make_box_geometry({-0.05f, -0.05f, -0.05f}, {0.05f, 0.05f, 0.05f}),
    {0.9f, 0.9f, 0.9f, 0.25f},
    {1, 1, 1, 0.35f},
    {0, 0, 0}};

static const GizmoComponent *translation_components[] = {
    &componentX,
    &componentY,
    &componentZ,
    &componentXY,
    &componentYZ,
    &componentZX,
    &componentXYZ,
};

static std::pair<const GizmoComponent *, float> raycast(const falg::Ray &ray)
{
    const GizmoComponent *updated_state = nullptr;
    float best_t = std::numeric_limits<float>::infinity();
    for (auto c : translation_components)
    {
        auto t = ray >> c->mesh;
        if (t < best_t)
        {
            updated_state = c;
            best_t = t;
        }
    }
    return std::make_pair(updated_state, best_t);
}

static void draw(Gizmo &gizmo, gizmo_system_impl *impl, const falg::Transform &t)
{
    for (auto c : translation_components)
    {
        gizmo_renderable r{
            .mesh = c->mesh,
            .color = (c == gizmo.active()) ? c->base_color : c->highlight_color,
        };
        for (auto &v : r.mesh.vertices)
        {
            v.position = t.ApplyPosition(v.position); // transform local coordinates into worldspace
            v.normal = t.ApplyDirection(v.normal);
        }
        impl->drawlist.push_back(r);
    }
}

namespace handle
{
bool translation(const GizmoSystem &ctx, uint32_t id, bool is_local,
                 const falg::Transform *parent, falg::float3 &t, const falg::float4 &r)
{
    auto &impl = ctx.m_impl;
    auto [gizmo, created] = impl->get_or_create_gizmo(id);

    // raycast
    auto worldRay = falg::Ray{impl->state.ray_origin, impl->state.ray_direction};
    auto gizmoTransform = falg::Transform{t, r};
    // auto gizmoTransform = falg::Transform{t, is_local ? r : falg::float4{0, 0, 0, 1}};
    if (parent)
    {
        // local to world
        gizmoTransform = gizmoTransform * *parent;
    }
    if (!is_local)
    {
        gizmoTransform.rotation = {0, 0, 0, 1};
    }
    auto localRay = worldRay.Transform(gizmoTransform.Inverse());
    auto [mesh, best_t] = raycast(localRay);
    gizmo->hover(mesh != nullptr);

    // update
    if (impl->state.has_clicked)
    {
        if (mesh)
        {
            auto localHit = localRay.SetT(best_t);
            auto worldOffset = gizmoTransform.ApplyPosition(localHit) - gizmoTransform.translation;
            falg::float3 axis;
            if (mesh == &componentXYZ)
            {
                axis = -falg::QuaternionZDir(impl->state.camera_rotation);
            }
            else
            {
                if (is_local)
                {
                    axis = gizmoTransform.ApplyDirection(mesh->axis);
                }
                else
                {
                    axis = mesh->axis;
                }
            }
            gizmo->begin(mesh, worldOffset, {gizmoTransform.translation, {0, 0, 0, 1}, {1, 1, 1}}, axis);
        }
    }
    else if (impl->state.has_released)
    {
        gizmo->end();
    }
    else
    {
        // drag
        auto active = gizmo->active();
        if (active)
        {
            if (active == &componentX || active == &componentY || active == &componentZ)
            {
                axisDragger(*active, worldRay, gizmo->m_state, &gizmoTransform.translation, gizmo->m_state.axis);
            }
            else
            {
                planeDragger(*active, worldRay, gizmo->m_state, &gizmoTransform.translation, gizmo->m_state.axis);
            }
            if (parent)
            {
                // world to local
                t = (gizmoTransform * parent->Inverse()).translation;
            }
            else
            {
                t = gizmoTransform.translation;
            }
        }
    }

    // draw
    draw(*gizmo, impl, gizmoTransform);

    return gizmo->isHoverOrActive();
}

} // namespace handle
} // namespace wgut::gizmo
