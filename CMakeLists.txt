cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "Kraft")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(${PROJECT_NAME} LANGUAGES CXX C)

set(KRAFT_PATH ${CMAKE_SOURCE_DIR}/kraft)
set(KRAFT_APP_TYPE "GUI")

include(${KRAFT_PATH}/cmake/kraft_helpers.cmake)

set(KRAFT_BUILDING_BASE TRUE)

add_subdirectory(kraft ${KRAFT_BINARY_DIR}/kraft/core)
add_subdirectory(testbed ${KRAFT_BINARY_DIR}/testbed)
# add_subdirectory(tools/shader_compiler ${KRAFT_BINARY_DIR}/tools/shader_compiler)