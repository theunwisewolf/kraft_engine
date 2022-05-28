cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "GameEngine")
project(${PROJECT_NAME})

set(SRC_FILES 
    src/main.cpp
    src/shader.cpp
    src/texture.cpp

    # vendor/glad/src/glad.c
)

set(HEADER_FILES
    src/main.h
    src/shader.h
    src/util.h
    src/texture.h
)

# list(APPEND SRC_FILES src/main.cpp)
# list(APPEND HEADER_FILES src/main.h)

set(CODE_FILES ${SRC_FILES} ${HEADER_FILES})

add_subdirectory(vendor/glfw)
add_executable(${PROJECT_NAME} ${CODE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE vendor)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glad/include)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glfw/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)