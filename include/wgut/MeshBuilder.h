#pragma once
#include "wgut_shader.h"

namespace wgut
{

class MeshBuilder
{
    shader::InputLayoutPtr m_layout;

public:
    MeshBuilder(const shader::InputLayoutPtr &layout)
        : m_layout(layout)
    {
    }
};

} // namespace wgut
