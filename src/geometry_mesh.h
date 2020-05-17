#pragma once
#include <vector>
#include <falg.h>

namespace wgut::gizmo
{

struct geometry_vertex
{
    falg::float3 position;
    falg::float3 normal;
    falg::float4 color;
};

struct geometry_mesh
{
    std::vector<geometry_vertex> vertices;
    std::vector<uint32_t> triangles;

    static geometry_mesh make_box_geometry(const falg::float3 &min_bounds, const falg::float3 &max_bounds);
    static geometry_mesh make_cylinder_geometry(const falg::float3 &axis, const falg::float3 &arm1, const falg::float3 &arm2, uint32_t slices);
    static geometry_mesh make_lathed_geometry(const falg::float3 &axis, const falg::float3 &arm1, const falg::float3 &arm2, int slices, const falg::float2 *points, uint32_t pointCount, const float eps = 0.0f);

    void compute_normals();

    void clear()
    {
        vertices.clear();
        triangles.clear();
    }
};

float operator>>(const falg::Ray &ray, const geometry_mesh &mesh);

} // namespace wgut::gizmo
