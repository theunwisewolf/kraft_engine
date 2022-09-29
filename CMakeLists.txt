cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME "Kraft")
set(CMAKE_CXX_STANDARD 14)
project(${PROJECT_NAME})

# Vulkan
find_package(Vulkan)
	
# Try extracting VulkanSDK path from ${Vulkan_INCLUDE_DIRS}
if (NOT ${Vulkan_INCLUDE_DIRS} STREQUAL "")
    set(VULKAN_PATH ${Vulkan_INCLUDE_DIRS})
    STRING(REGEX REPLACE "/Include" "" VULKAN_PATH ${VULKAN_PATH})
endif()
        
if(NOT Vulkan_FOUND)
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
    src/main.cpp
    # src/shader.cpp
    # src/texture.cpp
    src/containers/array.cpp
    src/core/kraft_log.cpp
    src/core/kraft_input.cpp
    src/core/kraft_events.cpp
    src/core/kraft_application.cpp
    src/core/kraft_time.cpp
    src/math/kraft_math.cpp
    src/platform/windows/kraft_win32.cpp
    src/platform/windows/kraft_win32_window.cpp

    # Renderer
    src/renderer/kraft_renderer_backend.cpp
    src/renderer/kraft_renderer_frontend.cpp
    src/renderer/vulkan/kraft_vulkan_backend.cpp
    src/renderer/vulkan/kraft_vulkan_device.cpp

    # imgui
    # vendor/imgui/imgui.cpp
    # vendor/imgui/imgui_draw.cpp
    # vendor/imgui/imgui_widgets.cpp
    # vendor/imgui/imgui_tables.cpp
    # vendor/imgui/imgui_demo.cpp
    # vendor/imgui/backends/imgui_impl_opengl3.cpp
    # vendor/imgui/backends/imgui_impl_glfw.cpp
    # vendor/glad/src/glad.c
)

set(HEADER_FILES
    src/main.h
    # src/shader.h
    src/util.h
    # src/texture.h
    src/core/kraft_main.h
    src/core/kraft_core.h
    src/core/kraft_log.h
    src/core/kraft_memory.h
    src/core/kraft_asserts.h
    src/core/kraft_application.h
    src/math/kraft_math.h
    src/platform/kraft_platform.h
    src/platform/windows/kraft_win32_window.h
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
add_executable(${PROJECT_NAME} ${CODE_FILES})

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE vendor)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glad/include)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/glfw/include)
target_include_directories(${PROJECT_NAME} PRIVATE vendor/imgui)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

# Compile time defines
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# find_package(OpenGL REQUIRED)

# Library links
# target_link_directories(${PROJECT_NAME} PRIVATE ${VULKAN_PATH}/Bin;${VULKAN_PATH}/Lib;)
target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES})
# target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY})

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)