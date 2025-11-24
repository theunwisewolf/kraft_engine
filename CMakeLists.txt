cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "Kraft")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(${PROJECT_NAME} LANGUAGES CXX C)

set(KRAFT_PATH ${CMAKE_SOURCE_DIR}/kraft)
set(KRAFT_APP_TYPE "GUI")

include(${KRAFT_PATH}/cmake/kraft_helpers.cmake)

set(KRAFT_BUILDING_BASE TRUE)

add_subdirectory(kraft ${KRAFT_BINARY_DIR}/kraft/core)
add_subdirectory(editor ${KRAFT_BINARY_DIR}/editor)
add_subdirectory(sample_app ${KRAFT_BINARY_DIR}/sample_app)
add_subdirectory(text_drawing ${KRAFT_BINARY_DIR}/text_drawing)
# add_subdirectory(tools/shader_compiler ${KRAFT_BINARY_DIR}/tools/shader_compiler)