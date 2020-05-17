get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" TARGET_NAME ${TARGET_NAME})
add_executable(${TARGET_NAME}
    main.cpp
    )
target_compile_definitions(${TARGET_NAME}
PRIVATE
    WINDOW_NAME="${TARGET_NAME}"
    )
target_link_libraries(${TARGET_NAME}
PRIVATE
    wgut
    )
