#include <wgut/wgut_gizmo.h>
#include "impl.h"
#include "assert.h"

namespace wgut::gizmo
{

static bool dragger(const GizmoComponent &component,
                    const falg::Ray &worldRay, const GizmoState &state,
                    falg::Transform *out, bool is_local)
{
    auto start_orientation = is_local ? state.original.rotation : falg::float4{0, 0, 0, 1};
    falg::Transform original_pose = {state.original.translation, start_orientation};
    auto the_axis = original_pose.ApplyDirection(component.axis);
    auto t = worldRay >> falg::Plane{the_axis, state.original.translation + state.offset};
    if (t < 0)
    {
        return false;
    }

    auto center_of_rotation = state.original.translation + the_axis * falg::Dot(the_axis, state.offset);
    auto arm1 = falg::Normalize(state.original.translation + state.offset - center_of_rotation);
    auto arm2 = falg::Normalize(worldRay.SetT(t) - center_of_rotation);
    float d = falg::Dot(arm1, arm2);
    if (d > 0.999f)
    {
        return false;
    }

    float angle = std::acos(d);
    if (angle < 0.001f)
    {
        return false;
    }

    auto a = falg::Normalize(falg::Cross(arm1, arm2));
    out->rotation = falg::QuaternionMul(
        start_orientation,
        falg::QuaternionAxisAngle(a, angle));
    return true;
}

static falg::float2 ring_points[] = {{+0.025f, 1}, {-0.025f, 1}, {-0.025f, 1}, {-0.025f, 1.1f}, {-0.025f, 1.1f}, {+0.025f, 1.1f}, {+0.025f, 1.1f}, {+0.025f, 1}};

static GizmoComponent componentX{
    geometry_mesh::make_lathed_geometry({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, 32, ring_points, _countof(ring_points), 0.003f),
    {1, 0.5f, 0.5f, 1.f},
    {1, 0, 0, 1.f},
    {1, 0, 0},
};
static GizmoComponent componentY{
    geometry_mesh::make_lathed_geometry({0, 1, 0}, {0, 0, 1}, {1, 0, 0}, 32, ring_points, _countof(ring_points), -0.003f),
    {0.5f, 1, 0.5f, 1.f},
    {0, 1, 0, 1.f},
    {0, 1, 0},
};
static GizmoComponent componentZ{
    geometry_mesh::make_lathed_geometry({0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 32, ring_points, _countof(ring_points)),
    {0.5f, 0.5f, 1, 1.f},
    {0, 0, 1, 1.f},
    {0, 0, 1},
};

static const GizmoComponent *orientation_components[] = {
    &componentX,
    &componentY,
    &componentZ,
};

inline std::pair<const GizmoComponent *, float> raycast(const falg::Ray &ray)
{
    const GizmoComponent *updated_state = nullptr;
    float best_t = std::numeric_limits<float>::infinity();
    for (auto c : orientation_components)
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

static void draw_global_active(std::vector<gizmo_renderable> &drawlist,
                               const falg::Transform &gizmoTransform, const GizmoComponent *active,
                               const GizmoState &state)
{
    // auto modelMatrix = falg::size_cast<minalg::float4x4>(gizmoTransform.matrix());

    // For non-local transformations, we only present one rotation ring
    // and draw an arrow from the center of the gizmo to indicate the degree of rotation
    gizmo_renderable r{
        .mesh = active->mesh,
        .color = active->base_color,
    };
    for (auto &v : r.mesh.vertices)
    {
        v.position = gizmoTransform.ApplyPosition(v.position); // transform local coordinates into worldspace
        v.normal = gizmoTransform.ApplyDirection(v.normal);
    }
    drawlist.push_back(r);

    {
        // Create orthonormal basis for drawing the arrow
        auto a = gizmoTransform.ApplyDirection(state.offset);
        auto zDir = falg::Normalize(active->axis);
        auto xDir = falg::Normalize(falg::Cross(a, zDir));
        auto yDir = falg::Cross(zDir, xDir);

        // Ad-hoc geometry
        falg::float2 arrow_points[] = {{0.0f, 0.f}, {0.0f, 0.05f}, {0.8f, 0.05f}, {0.9f, 0.10f}, {1.0f, 0}};
        auto geo = geometry_mesh::make_lathed_geometry(
            falg::size_cast<falg::float3>(yDir),
            falg::size_cast<falg::float3>(xDir),
            falg::size_cast<falg::float3>(zDir),
            32, arrow_points, _countof(arrow_points));

        gizmo_renderable r;
        r.mesh = geo;
        r.color = falg::float4{1, 1, 1, 1};
        for (auto &v : r.mesh.vertices)
        {
            v.position = gizmoTransform.ApplyPosition(v.position);
            v.normal = gizmoTransform.ApplyDirection(v.normal);
        }
        drawlist.push_back(r);
    }
}

static void draw(std::vector<gizmo_renderable> &drawlist, const falg::Transform &gizmoTransform, const GizmoComponent *active)
{
    for (auto mesh : orientation_components)
    {
        gizmo_renderable r{
            .mesh = mesh->mesh,
            .color = (mesh == active) ? mesh->base_color : mesh->highlight_color,
        };
        for (auto &v : r.mesh.vertices)
        {
            v.position = gizmoTransform.ApplyPosition(v.position);
            v.normal = gizmoTransform.ApplyDirection(v.normal);
        }
        drawlist.push_back(r);
    }
}

namespace handle
{

bool rotation(const GizmoSystem &ctx, uint32_t id, bool is_local,
              const falg::Transform *parent, const falg::float3 &t, falg::float4 &r)
{
    auto &impl = ctx.m_impl;
    auto [gizmo, created] = impl->get_or_create_gizmo(id);

    // assert(length2(t.orientation) > float(1e-6));
    auto worldRay = falg::Ray{impl->state.ray_origin, impl->state.ray_direction};
    auto gizmoTransform = falg::Transform{t, r}; // Orientation is local by default
    if (parent)
    {
        gizmoTransform = gizmoTransform * *parent;
    }
    auto world = gizmoTransform;
    if (!is_local)
    {
        gizmoTransform.rotation = {0, 0, 0, 1};
    }

    // raycast
    {
        auto localRay = worldRay.Transform(gizmoTransform.Inverse());
        auto [mesh, best_t] = raycast(localRay);

        // update
        if (impl->state.has_clicked)
        {
            if (mesh)
            {
                auto localHit = localRay.SetT(best_t);
                auto worldOffset = gizmoTransform.ApplyPosition(localHit) - world.translation;
                gizmo->begin(mesh, worldOffset, {world.translation, world.rotation, {1, 1, 1}}, {});
            }
        }
        else if (impl->state.has_released)
        {
            gizmo->end();
        }
    }

    // drag
    auto active = gizmo->active();
    if (active)
    {
        dragger(*active, worldRay, gizmo->m_state, &gizmoTransform, is_local);
        if (!is_local)
        {
            r = falg::QuaternionMul(
                gizmo->m_state.original.rotation,
                gizmoTransform.rotation);
        }
        else
        {
            r = gizmoTransform.rotation;
        }
        if (parent)
        {
            r = falg::QuaternionMul(r, falg::QuaternionConjugate(parent->rotation));
        }
    }

    // draw
    if (!is_local && active)
    {
        draw_global_active(impl->drawlist, gizmoTransform, active, gizmo->m_state);
    }
    else
    {
        draw(impl->drawlist, gizmoTransform, active);
    }

    return gizmo->isHoverOrActive();
}

} // namespace handle
} // namespace wgut::gizmo
