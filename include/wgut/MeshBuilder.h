#pragma once
#include <gsl/span>
#include <stdexcept>
#include <stdint.h>
#include <assert.h>

namespace wgut::mesh
{

struct MeshBuilder
{
    uint32_t VertexStride = 0;
    std::vector<uint8_t> VerticesData;

    uint32_t IndexStride = 0;
    std::vector<uint8_t> IndicesData;

    MeshBuilder(uint32_t vertexStride, uint32_t indexStride)
        : VertexStride(vertexStride), IndexStride(indexStride)
    {
    }

    template <typename VERTEX>
    void AppendVertex(const VERTEX v)
    {
        VertexStride = sizeof(VERTEX);
        VerticesData.insert(VerticesData.end(), (const uint8_t *)&v, (const uint8_t *)&v + sizeof(VERTEX));
    }

    void AppendIndex(uint32_t i)
    {
        switch (IndexStride)
        {
        case 2:
        {
            auto index = static_cast<uint16_t>(i);
            IndicesData.insert(IndicesData.end(), (const uint8_t *)&index, (const uint8_t *)&index + 2);
        }
        break;

        case 4:
        {
            auto index = static_cast<uint32_t>(i);
            IndicesData.insert(IndicesData.end(), (const uint8_t *)&index, (const uint8_t *)&index + 4);
        }
        break;

        default:
            throw std::runtime_error("not implemented");
        }
    }

    template <typename VERTEX>
    void PushQuad(const VERTEX &v0, const VERTEX &v1, const VERTEX &v2, const VERTEX &v3)
    {
        // assert(VertexLayout.Stride() == sizeof(VERTEX));

        auto i = static_cast<uint32_t>(VerticesData.size() / sizeof(VERTEX));
        AppendVertex(v0);
        AppendVertex(v1);
        AppendVertex(v2);
        AppendVertex(v3);
        // t0
        AppendIndex(i);
        AppendIndex(i + 1);
        AppendIndex(i + 2);
        // t1
        AppendIndex(i + 2);
        AppendIndex(i + 3);
        AppendIndex(i);
    }
};

struct Quad
{
    struct float2
    {
        float x;
        float y;
    };
    struct Vertex
    {
        float2 position;
        float2 uv;
    };

    static MeshBuilder Create()
    {
        auto builder = MeshBuilder(sizeof(Vertex), 2);
        // clock wise
        builder.PushQuad(
            Vertex{{-1, 1}, {0, 0}},
            Vertex{{1, 1}, {1, 0}},
            Vertex{{1, -1}, {1, 1}},
            Vertex{{-1, -1}, {0, 1}}
            );
        return builder;
    }
};

struct Cube
{
    struct float2
    {
        float x;
        float y;
    };
    struct float3
    {
        float x;
        float y;
        float z;
    };
    struct Vertex
    {
        float3 position;
        float2 uv;
    };

    static MeshBuilder Create()
    {
        float3 positions[] = {
            {-1.0f, -1.0f, -1.0f},
            {1.0f, -1.0f, -1.0f},
            {1.0f, 1.0f, -1.0f},
            {-1.0f, 1.0f, -1.0f},
            {-1.0f, -1.0f, 1.0f},
            {1.0f, -1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {-1.0f, 1.0f, 1.0f},
        };

        float2 uv[] = {
            {0.0f, 1.0f},
            {1.0f, 1.0f},
            {1.0f, 0.0f},
            {0.0f, 0.0f},
        };

        auto builder = MeshBuilder(sizeof(Vertex), 2);
        builder.PushQuad(
            Vertex{positions[0], uv[0]},
            Vertex{positions[1], uv[1]},
            Vertex{positions[2], uv[2]},
            Vertex{positions[3], uv[3]});
        builder.PushQuad(
            Vertex{positions[1], uv[0]},
            Vertex{positions[5], uv[1]},
            Vertex{positions[6], uv[2]},
            Vertex{positions[2], uv[3]});
        builder.PushQuad(
            Vertex{positions[5], uv[0]},
            Vertex{positions[4], uv[1]},
            Vertex{positions[7], uv[2]},
            Vertex{positions[6], uv[3]});
        builder.PushQuad(
            Vertex{positions[4], uv[0]},
            Vertex{positions[0], uv[1]},
            Vertex{positions[3], uv[2]},
            Vertex{positions[7], uv[3]});
        builder.PushQuad(
            Vertex{positions[3], uv[0]},
            Vertex{positions[2], uv[1]},
            Vertex{positions[6], uv[2]},
            Vertex{positions[7], uv[3]});
        builder.PushQuad(
            Vertex{positions[0], uv[0]},
            Vertex{positions[4], uv[1]},
            Vertex{positions[5], uv[2]},
            Vertex{positions[1], uv[3]});

        return builder;
    }
};

} // namespace wgut
