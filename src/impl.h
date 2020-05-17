#pragma once
#include "gizmo.h"
#include <unordered_map>
#include <falg.h>

namespace wgut::gizmo
{

struct GizmoFrameState
{
    // Used for constructing inverse view projection for raycasting onto gizmo geometry
    std::array<float, 3> camera_position;
    std::array<float, 4> camera_rotation;

    // ray
    std::array<float, 3> ray_origin;
    std::array<float, 3> ray_direction;

    bool button{false};
    // State to describe if the user has pressed the left mouse button during the last frame
    bool has_clicked{false};
    // State to describe if the user has released the left mouse button during the last frame
    bool has_released{false};
};

struct gizmo_renderable
{
    geometry_mesh mesh;
    falg::float4 color;
};

struct gizmo_system_impl
{
private:
    geometry_mesh m_r{};
    std::unordered_map<uint32_t, std::unique_ptr<Gizmo>> m_gizmos;

public:
    std::vector<gizmo_renderable> drawlist;

    std::pair<Gizmo *, bool> get_or_create_gizmo(uint32_t id)
    {
        auto found = m_gizmos.find(id);
        bool created = false;
        if (found == m_gizmos.end())
        {
            // not found
            found = m_gizmos.insert(std::make_pair(id, std::make_unique<Gizmo>())).first;
            created = true;
        }
        return std::make_pair(found->second.get(), created);
    }

    GizmoFrameState state;

    // Public methods
    void gizmo_system_impl::update(const GizmoFrameState &state)
    {
        auto lastButton = this->state.button;
        this->state = state;
        this->state.has_clicked = !lastButton && state.button;
        this->state.has_released = lastButton && !state.button;

        drawlist.clear();
        m_r.clear();
    }

    const geometry_mesh &render()
    {
        // Combine all gizmo sub-meshes into one super-mesh
        for (auto &m : drawlist)
        {
            uint32_t offset = (uint32_t)m_r.vertices.size();
            auto it = m_r.vertices.insert(m_r.vertices.end(), m.mesh.vertices.begin(), m.mesh.vertices.end());
            for (auto tri = m.mesh.triangles.begin(); tri != m.mesh.triangles.end(); tri += 3)
            {
                m_r.triangles.push_back(offset + tri[0]);
                m_r.triangles.push_back(offset + tri[1]);
                m_r.triangles.push_back(offset + tri[2]);
            }
            for (; it != m_r.vertices.end(); ++it)
                it->color = m.color; // Take the color and shove it into a per-vertex attribute
        }

        return m_r;
    }
};

} // namespace  gizmesh
