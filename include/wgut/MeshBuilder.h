#pragma once
#include "wgut_shader.h"

namespace wgut
{

template <typename VERTEX, typename INDEX = uint16_t>
struct MeshBuilder
{
    std::vector<VERTEX> Vertices;
    static const UINT Stride = sizeof(VERTEX);
    std::vector<INDEX> Indices;
    static const UINT IndexStride = sizeof(INDEX);

    void
    PushQuad(const VERTEX &v0, const VERTEX &v1, const VERTEX &v2, const VERTEX &v3)
    {
        auto i = static_cast<INDEX>(Vertices.size());
        Vertices.push_back(v0);
        Vertices.push_back(v1);
        Vertices.push_back(v2);
        Vertices.push_back(v3);
        // t0
        Indices.push_back(i);
        Indices.push_back(i + 1);
        Indices.push_back(i + 2);
        // t1
        Indices.push_back(i + 2);
        Indices.push_back(i + 3);
        Indices.push_back(i);
    }
};

} // namespace wgut
