get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" TARGET_NAME ${TARGET_NAME})
add_executable(${TARGET_NAME}
    main.cpp
    )
set_property(TARGET ${TARGET_NAME}
PROPERTY 
    CXX_STANDARD 20
    )
target_link_libraries(${TARGET_NAME}
PRIVATE
    wgut
    imgui
    )
