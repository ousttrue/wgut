set(TARGET_NAME gsl)
add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME}
INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/../_externals/GSL-3.0.1/include
    )
