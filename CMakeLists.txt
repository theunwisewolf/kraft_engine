cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "Kraft")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(${PROJECT_NAME} LANGUAGES CXX C)

# Vulkan
find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

# Try extracting VulkanSDK path from ${Vulkan_INCLUDE_DIRS}
if (NOT ${Vulkan_INCLUDE_DIRS} STREQUAL "")
    set(VULKAN_PATH ${Vulkan_INCLUDE_DIRS})
    STRING(REGEX REPLACE "/Include" "" VULKAN_PATH ${VULKAN_PATH})
endif()
        
if (NOT Vulkan_FOUND)
    # CMake may fail to locate the libraries but could be able to 
    # provide some path in Vulkan SDK include directory variable
    # 'Vulkan_INCLUDE_DIRS', try to extract path from this.
    message(STATUS "Failed to locate Vulkan SDK, retrying again...")
    if(EXISTS "${VULKAN_PATH}")
        message(STATUS "Successfully located the Vulkan SDK: ${VULKAN_PATH}")
    else()
        message("Error: Unable to locate Vulkan SDK. Please turn off auto locate option by specifying 'AUTO_LOCATE_VULKAN' as 'OFF'")
        message("and specify manually path using 'echo ' and 'VULKAN_VERSION' variables in the CMakeLists.txt.")
        return()
    endif()
endif()

set(SRC_FILES 
    src/containers/array.cpp
    src/core/kraft_log.cpp
    src/core/kraft_input.cpp
    src/core/kraft_events.cpp
    src/core/kraft_application.cpp
    src/core/kraft_time.cpp
    src/core/kraft_memory.cpp
    src/math/kraft_math.cpp
    src/platform/kraft_window.cpp
    src/platform/kraft_filesystem.cpp
    src/platform/windows/kraft_win32.cpp
    
    # Renderer
    src/renderer/kraft_camera.cpp
    src/renderer/kraft_renderer_backend.cpp
    src/renderer/kraft_renderer_frontend.cpp
    src/renderer/kraft_renderer_imgui.cpp
    src/renderer/vulkan/kraft_vulkan_backend.cpp
    src/renderer/vulkan/kraft_vulkan_device.cpp
    src/renderer/vulkan/kraft_vulkan_swapchain.cpp
    src/renderer/vulkan/kraft_vulkan_image.cpp
    src/renderer/vulkan/kraft_vulkan_renderpass.cpp
    src/renderer/vulkan/kraft_vulkan_command_buffer.cpp
    src/renderer/vulkan/kraft_vulkan_framebuffer.cpp
    src/renderer/vulkan/kraft_vulkan_fence.cpp
    src/renderer/vulkan/kraft_vulkan_shader.cpp
    src/renderer/vulkan/kraft_vulkan_pipeline.cpp
    src/renderer/vulkan/kraft_vulkan_buffer.cpp
    src/renderer/vulkan/kraft_vulkan_index_buffer.cpp
    src/renderer/vulkan/kraft_vulkan_vertex_buffer.cpp
    src/renderer/vulkan/kraft_vulkan_imgui.cpp
    src/renderer/vulkan/kraft_vulkan_texture.cpp

    # Systems
    src/systems/kraft_texture_system.cpp
    src/systems/kraft_material_system.cpp
    src/systems/kraft_geometry_system.cpp

    # imgui
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_impl_vulkan.cpp
    vendor/imgui/imgui_impl_glfw.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_demo.cpp

    # test stuff
    src/renderer/vulkan/tests/simple_scene.cpp
)

set(HEADER_FILES
    src/util.h
    src/core/kraft_main.h
    src/core/kraft_core.h
    src/core/kraft_log.h
    src/core/kraft_memory.h
    src/core/kraft_asserts.h
    src/core/kraft_application.h
    src/math/kraft_math.h
    src/platform/kraft_platform.h
)

# list(APPEND SRC_FILES src/main.cpp)
# list(APPEND HEADER_FILES src/main.h)

add_subdirectory(vendor/glfw)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
if (APPLE)
    list(APPEND SRC_FILES src/platform/macos/kraft_macos.mm)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK")
endif()

set(CODE_FILES ${SRC_FILES} ${HEADER_FILES})
add_library(${PROJECT_NAME} STATIC ${CODE_FILES})

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/imgui)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glad/include)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glfw/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})

# Compile time defines
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# If building a shared lib
# target_compile_definitions(${PROJECT_NAME} PRIVATE KRAFT_SHARED)

# If building a static lib
target_compile_definitions(${PROJECT_NAME} PUBLIC KRAFT_STATIC)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# find_package(OpenGL REQUIRED)

# Library links
# target_link_directories(${PROJECT_NAME} PRIVATE ${VULKAN_PATH}/Bin;${VULKAN_PATH}/Lib;)
target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES})
# target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

add_subdirectory(testbed)