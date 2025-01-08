cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "Kraft")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(${PROJECT_NAME} LANGUAGES CXX C)

set(SRC_FILES 
    src/containers/kraft_carray.cpp
    src/containers/kraft_array.cpp
    src/containers/kraft_buffer.cpp
    src/core/kraft_allocators.cpp
    src/core/kraft_log.cpp
    src/core/kraft_asserts.cpp
    src/core/kraft_input.cpp
    src/core/kraft_events.cpp
    src/core/kraft_engine.cpp
    src/core/kraft_time.cpp
    src/core/kraft_memory.cpp
    src/core/kraft_string.cpp
    src/core/kraft_string_view.cpp
    src/core/kraft_lexer.cpp
    src/core/kraft_hash.cpp
    src/math/kraft_math.cpp
    src/platform/kraft_filesystem.cpp
    src/platform/kraft_platform_common.cpp
    src/platform/windows/kraft_win32.cpp

    src/renderer/shaderfx/kraft_shaderfx.cpp
)

if (WIN32)
    list(APPEND SRC_FILES src/platform/windows/kraft_win32_filesystem.cpp)
endif()

set(HEADER_FILES
    src/util.h
    src/core/kraft_core.h
    src/core/kraft_log.h
    src/core/kraft_memory.h
    src/core/kraft_asserts.h
    src/math/kraft_math.h
    src/platform/kraft_platform.h
)


if (KRAFT_APP_TYPE STREQUAL "GUI")
    list(APPEND HEADER_FILES
        # Platform
        src/platform/kraft_window.cpp
    
        # Renderer
        src/renderer/kraft_camera.cpp
        src/renderer/kraft_renderer_frontend.cpp
        src/renderer/kraft_resource_manager.cpp
        src/renderer/kraft_resource_pool.cpp
        src/renderer/vulkan/kraft_vulkan_volk_impl.cpp
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
        src/renderer/vulkan/kraft_vulkan_imgui.cpp
        src/renderer/vulkan/kraft_vulkan_resource_manager.cpp
        src/renderer/vulkan/kraft_vulkan_helpers.cpp

        src/renderer/renderables/kraft_gameobject.cpp

        # Systems
        src/systems/kraft_texture_system.cpp
        src/systems/kraft_material_system.cpp
        src/systems/kraft_geometry_system.cpp
        src/systems/kraft_shader_system.cpp
        src/systems/kraft_asset_database.cpp

        # imgui
        vendor/imgui/imgui.cpp
        vendor/imgui/backends/imgui_impl_vulkan.cpp
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_demo.cpp

        # imgui extensions
        vendor/imgui/extensions/imguizmo/ImGuizmo.cpp

        # World
        src/world/kraft_world.cpp
        src/world/kraft_entity.cpp
        src/world/kraft_components.cpp
    )
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
if (APPLE)
    list(APPEND SRC_FILES src/platform/macos/kraft_macos.mm)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK")
elseif (UNIX)
    list(APPEND SRC_FILES src/platform/linux/kraft_linux.cpp)
endif()

set(CODE_FILES ${SRC_FILES} ${HEADER_FILES})
add_library(${PROJECT_NAME} STATIC ${CODE_FILES})

# Include dirs
target_include_directories(${PROJECT_NAME} PUBLIC src ../res)
target_include_directories(${PROJECT_NAME} PUBLIC ${KRAFT_STATIC_LIBS_INCLUDE_PATHS})

# Compile time defines
target_compile_definitions(${PROJECT_NAME} PUBLIC ${KRAFT_STATIC_LIBS_COMPILE_DEFINITIONS} ${KRAFT_COMPILE_DEFINITIONS})

# Libraries
target_link_libraries(${PROJECT_NAME} ${KRAFT_STATIC_LIBS})

# Disable warnings from linking shaderc
if (MSVC)
target_link_options(${PROJECT_NAME} INTERFACE "/ignore:4099")
endif()

# If building a shared lib
# target_compile_definitions(${PROJECT_NAME} PRIVATE KRAFT_SHARED)

# If building a static lib
target_compile_definitions(${PROJECT_NAME} PUBLIC KRAFT_STATIC)

# Compiler options
if (MSVC)
# target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-function)
else()
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unused-function -Wno-unused-variable)
endif()

# find_package(OpenGL REQUIRED)

set(KRAFT_LIBRARY_FOLDER_NAME ${KRAFT_LIBRARY_OS_PREFIX}-${KRAFT_LIBRARY_BUILD_TYPE_PREFIX}-${KRAFT_LIBRARY_COMPILER_PREFIX}-${KRAFT_LIBRARY_ARCH_PREFIX})

# add_library(shaderc_combined STATIC IMPORTED)
# set_property(TARGET shaderc_combined PROPERTY IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/vendor/shaderc/build/${KRAFT_LIBRARY_FOLDER_NAME}/shaderc_combined.lib")

# ShaderC
# target_link_libraries(${PROJECT_NAME} C:/VulkanSDK/1.3.268.0/Lib/shaderc_combinedd.lib)
# target_include_directories(${PROJECT_NAME} PUBLIC vendor/shaderc/src/libshaderc/include)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
