cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "KraftShaderCompiler")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${PROJECT_NAME})

set(KRAFT_APP_TYPE "Console")

if (NOT KRAFT_BUILDING_BASE)
    set(KRAFT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../kraft)
    include(${KRAFT_PATH}/cmake/kraft_helpers.cmake)
    get_filename_component(KRAFT_PATH "${KRAFT_PATH}" ABSOLUTE)
    add_subdirectory(${KRAFT_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

set(SRC_FILES 
    src/main.cpp
)

if (WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
else()
add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ../src)
target_compile_definitions(${PROJECT_NAME} PUBLIC KRAFT_STATIC)

# add_library(Kraft STATIC IMPORTED)
# set_target_properties(Kraft PROPERTIES
#     IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../bin/Kraft.lib
#     INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../../src"
# )

target_link_libraries(${PROJECT_NAME} Kraft)

if (KRAFT_DEBUG_BUILD)
    target_link_libraries(${PROJECT_NAME} ${Vulkan_shaderc_combined_DEBUG_LIBRARY})
else()
    target_link_libraries(${PROJECT_NAME} ${Vulkan_shaderc_combined_LIBRARY})
endif()

# target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)