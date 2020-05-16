set(TARGET_NAME wgut)
add_library(${TARGET_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/../src/Win32Window.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/ImGuiImplScreenState.cpp
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
