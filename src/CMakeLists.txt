set(TARGET_NAME wgut)
add_library(${TARGET_NAME}
    Win32Window.cpp
    gizmo.cpp
    gizmo_translation.cpp
    gizmo_rotation.cpp
    gizmo_scale.cpp
    geometry_mesh.cpp
    )
set_property(TARGET ${TARGET_NAME}
PROPERTY 
    CXX_STANDARD 20
    )
target_include_directories(${TARGET_NAME} 
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
    )
target_link_libraries(${TARGET_NAME} 
PUBLIC
    d3d11
    dxgi
    d3dcompiler
    d2d1
    gsl
    imgui
    )
