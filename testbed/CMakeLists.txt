cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "KraftTestBed")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${PROJECT_NAME})

set(KRAFT_APP_TYPE "GUI")

if (NOT KRAFT_BUILDING_BASE)
    set(KRAFT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../kraft)
    get_filename_component(KRAFT_PATH "${KRAFT_PATH}" ABSOLUTE)
    add_subdirectory(${KRAFT_PATH} ${KRAFT_BINARY_DIR}/kraft/core)
endif()

set(SRC_FILES 
    src/scenes/simple_scene.cpp
    src/widgets.cpp
    src/main.cpp
)

if (WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
else()
add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

target_link_libraries(${PROJECT_NAME} Kraft)

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/tools/compile_shaders 
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)