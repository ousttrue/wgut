#include "geometry_mesh.h"

static const float tau = 6.28318530718f;

namespace wgut::gizmo
{

void geometry_mesh::compute_normals()
{
    static const double NORMAL_EPSILON = 0.0001;

    std::vector<uint32_t> uniqueVertIndices(this->vertices.size(), 0);
    for (uint32_t i = 0; i < uniqueVertIndices.size(); ++i)
    {
        if (uniqueVertIndices[i] == 0)
        {
            uniqueVertIndices[i] = i + 1;
            auto v0 = this->vertices[i].position;
            for (auto j = i + 1; j < this->vertices.size(); ++j)
            {
                auto v1 = this->vertices[j].position;
                if (falg::Length(v1 - v0) < NORMAL_EPSILON)
                {
                    uniqueVertIndices[j] = uniqueVertIndices[i];
                }
            }
        }
    }

    uint32_t idx0, idx1, idx2;
    for (auto t = triangles.begin(); t != triangles.end(); t += 3)
    {
        idx0 = uniqueVertIndices[t[0]] - 1;
        idx1 = uniqueVertIndices[t[1]] - 1;
        idx2 = uniqueVertIndices[t[2]] - 1;

        geometry_vertex &v0 = this->vertices[idx0], &v1 = this->vertices[idx1], &v2 = this->vertices[idx2];
        auto n = falg::Cross(v1.position - v0.position, v2.position - v0.position);
        v0.normal += n;
        v1.normal += n;
        v2.normal += n;
    }

    for (uint32_t i = 0; i < this->vertices.size(); ++i)
        this->vertices[i].normal = this->vertices[uniqueVertIndices[i] - 1].normal;
    for (geometry_vertex &v : this->vertices)
        v.normal = falg::Normalize(v.normal);
}

geometry_mesh geometry_mesh::make_box_geometry(const falg::float3 &min_bounds, const falg::float3 &max_bounds)
{
    auto a = min_bounds;
    auto b = max_bounds;
    geometry_mesh mesh;
    mesh.vertices = {
        {{a[0], a[1], a[2]}, {-1, 0, 0}},
        {{a[0], a[1], b[2]}, {-1, 0, 0}},
        {{a[0], b[1], b[2]}, {-1, 0, 0}},
        {{a[0], b[1], a[2]}, {-1, 0, 0}},
        {{b[0], a[1], a[2]}, {+1, 0, 0}},
        {{b[0], b[1], a[2]}, {+1, 0, 0}},
        {{b[0], b[1], b[2]}, {+1, 0, 0}},
        {{b[0], a[1], b[2]}, {+1, 0, 0}},
        {{a[0], a[1], a[2]}, {0, -1, 0}},
        {{b[0], a[1], a[2]}, {0, -1, 0}},
        {{b[0], a[1], b[2]}, {0, -1, 0}},
        {{a[0], a[1], b[2]}, {0, -1, 0}},
        {{a[0], b[1], a[2]}, {0, +1, 0}},
        {{a[0], b[1], b[2]}, {0, +1, 0}},
        {{b[0], b[1], b[2]}, {0, +1, 0}},
        {{b[0], b[1], a[2]}, {0, +1, 0}},
        {{a[0], a[1], a[2]}, {0, 0, -1}},
        {{a[0], b[1], a[2]}, {0, 0, -1}},
        {{b[0], b[1], a[2]}, {0, 0, -1}},
        {{b[0], a[1], a[2]}, {0, 0, -1}},
        {{a[0], a[1], b[2]}, {0, 0, +1}},
        {{b[0], a[1], b[2]}, {0, 0, +1}},
        {{b[0], b[1], b[2]}, {0, 0, +1}},
        {{a[0], b[1], b[2]}, {0, 0, +1}},
    };
    std::array<uint32_t, 3> triangles[] = {{0, 1, 2}, {0, 2, 3}, {4, 5, 6}, {4, 6, 7}, {8, 9, 10}, {8, 10, 11}, {12, 13, 14}, {12, 14, 15}, {16, 17, 18}, {16, 18, 19}, {20, 21, 22}, {20, 22, 23}};
    auto begin = (uint32_t *)triangles;
    auto end = begin + _countof(triangles) * 3;
    mesh.triangles.assign(begin, end);
    return mesh;
}

geometry_mesh geometry_mesh::make_cylinder_geometry(const falg::float3 &axis, const falg::float3 &arm1, const falg::float3 &arm2, uint32_t slices)
{
    // Generated curved surface
    geometry_mesh mesh;

    for (uint32_t i = 0; i <= slices; ++i)
    {
        const float tex_s = static_cast<float>(i) / slices, angle = (float)(i % slices) * tau / slices;
        auto arm = arm1 * std::cos(angle) + arm2 * std::sin(angle);
        mesh.vertices.push_back({arm, falg::Normalize(arm)});
        mesh.vertices.push_back({arm + axis, falg::Normalize(arm)});
    }
    for (uint32_t i = 0; i < slices; ++i)
    {
        mesh.triangles.push_back(i * 2);
        mesh.triangles.push_back(i * 2 + 2);
        mesh.triangles.push_back(i * 2 + 3);

        mesh.triangles.push_back(i * 2);
        mesh.triangles.push_back(i * 2 + 3);
        mesh.triangles.push_back(i * 2 + 1);
    }

    // Generate caps
    uint32_t base = (uint32_t)mesh.vertices.size();
    for (uint32_t i = 0; i < slices; ++i)
    {
        const float angle = static_cast<float>(i % slices) * tau / slices, c = std::cos(angle), s = std::sin(angle);
        auto arm = arm1 * c + arm2 * s;
        mesh.vertices.push_back({arm + axis, falg::Normalize(axis)});
        mesh.vertices.push_back({arm, -falg::Normalize(axis)});
    }
    for (uint32_t i = 2; i < slices; ++i)
    {
        mesh.triangles.push_back(base);
        mesh.triangles.push_back(base + i * 2 - 2);
        mesh.triangles.push_back(base + i * 2);

        mesh.triangles.push_back(base + 1);
        mesh.triangles.push_back(base + i * 2 + 1);
        mesh.triangles.push_back(base + i * 2 - 1);
    }
    return mesh;
}

geometry_mesh geometry_mesh::make_lathed_geometry(const falg::float3 &axis, const falg::float3 &arm1, const falg::float3 &arm2, int slices,
                                                  const falg::float2 *points, uint32_t pointCount, const float eps)
{
    geometry_mesh mesh;
    for (int i = 0; i <= slices; ++i)
    {
        const float angle = (static_cast<float>(i % slices) * tau / slices) + (tau / 8.f);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        falg::Matrix2x3 mat{axis, arm1 * c + arm2 * s};
        for (uint32_t j = 0; j < pointCount; ++j)
        {
            auto &p = points[j];
            // 2D to 3D
            auto v = mat.Apply(p) + falg::float3{eps, eps, eps};
            mesh.vertices.push_back({v, {0, 0, 0}});
        }

        if (i > 0)
        {
            for (uint32_t j = 1; j < pointCount; ++j)
            {
                uint32_t i0 = (i - 1) * pointCount + (j - 1);
                uint32_t i1 = (i - 0) * pointCount + (j - 1);
                uint32_t i2 = (i - 0) * pointCount + (j - 0);
                uint32_t i3 = (i - 1) * pointCount + (j - 0);
                mesh.triangles.push_back(i0);
                mesh.triangles.push_back(i1);
                mesh.triangles.push_back(i2);

                mesh.triangles.push_back(i0);
                mesh.triangles.push_back(i2);
                mesh.triangles.push_back(i3);
            }
        }
    }
    mesh.compute_normals();
    return mesh;
}

float operator>>(const falg::Ray &ray, const geometry_mesh &mesh)
{
    float best_t = std::numeric_limits<float>::infinity();
    for (auto it = mesh.triangles.begin(); it != mesh.triangles.end(); it += 3)
    {
        auto t = ray >> falg::Triangle{
                            falg::size_cast<falg::float3>(mesh.vertices[it[0]].position),
                            falg::size_cast<falg::float3>(mesh.vertices[it[1]].position),
                            falg::size_cast<falg::float3>(mesh.vertices[it[2]].position)};
        if (t < best_t)
        {
            best_t = t;
        }
    }
    return best_t;
}

} // namespace wgut::gizmo