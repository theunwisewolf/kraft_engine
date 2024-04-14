set(KRAFT_BINARY_DIR ${PROJECT_BINARY_DIR}/kraft)

# Vulkan
find_package(Vulkan COMPONENTS glslc shaderc_combined)
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
    if (EXISTS "${VULKAN_PATH}")
        message(STATUS "Successfully located the Vulkan SDK: ${VULKAN_PATH}")
    else()
        message("Error: Unable to locate Vulkan SDK. Please turn off auto locate option by specifying 'AUTO_LOCATE_VULKAN' as 'OFF'")
        message("and specify manually path using 'echo ' and 'VULKAN_VERSION' variables in the CMakeLists.txt.")
        return()
    endif()
endif()

add_subdirectory(${KRAFT_PATH}/vendor/glfw ${KRAFT_BINARY_DIR}/vendor/glfw)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(KRAFT_LIBRARY_ARCH_PREFIX "x64")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(KRAFT_LIBRARY_ARCH_PREFIX "x86")
endif()

if (WIN32)
    set(KRAFT_LIBRARY_OS_PREFIX "windows")
elseif (APPLE)
    # TODO
    set(KRAFT_LIBRARY_OS_PREFIX "apple")
elseif (UNIX)
    set(KRAFT_LIBRARY_OS_PREFIX "linux")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(KRAFT_LIBRARY_COMPILER_PREFIX "clang")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(KRAFT_LIBRARY_COMPILER_PREFIX "msvc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(KRAFT_LIBRARY_COMPILER_PREFIX "gcc")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(KRAFT_LIBRARY_BUILD_TYPE_PREFIX "debug")
    set(KRAFT_DEBUG_BUILD ON)
else()
    set(KRAFT_LIBRARY_BUILD_TYPE_PREFIX "release")
    set(KRAFT_DEBUG_BUILD OFF)
endif()

set(KRAFT_STATIC_LIBS)
set(KRAFT_STATIC_LIBS_INCLUDE_PATHS vendor)
set(KRAFT_STATIC_LIBS_COMPILE_DEFINITIONS)
set(KRAFT_COMPILE_DEFINITIONS _ENABLE_EXTENDED_ALIGNED_STORAGE)

if (KRAFT_APP_TYPE STREQUAL "GUI")
    # Include paths
    list(APPEND KRAFT_STATIC_LIBS_INCLUDE_PATHS 
        ${KRAFT_PATH}/vendor/imgui 
        ${KRAFT_PATH}/vendor/glad/include 
        ${KRAFT_PATH}/vendor/glfw/include 
    )

    # Compile-time definitions
    list(APPEND KRAFT_STATIC_LIBS_COMPILE_DEFINITIONS GLFW_INCLUDE_NONE)

    # Libraries
    list(APPEND KRAFT_STATIC_LIBS glfw ${GLFW_LIBRARIES} ${Vulkan_LIBRARY})

    # Kraft compile time definitions
    list(APPEND KRAFT_COMPILE_DEFINITIONS KRAFT_GUI_APP)
else()
    # Kraft compile time definitions
    list(APPEND KRAFT_COMPILE_DEFINITIONS KRAFT_CONSOLE_APP)
endif()

# Common stuff
if (KRAFT_DEBUG_BUILD)
    list(APPEND KRAFT_STATIC_LIBS ${Vulkan_shaderc_combined_DEBUG_LIBRARY})
else()
    list(APPEND KRAFT_STATIC_LIBS ${Vulkan_shaderc_combined_LIBRARY})
endif()

list(APPEND KRAFT_STATIC_LIBS_INCLUDE_PATHS 
    ${Vulkan_INCLUDE_DIRS} 
)

if (WIN32)
    list(APPEND KRAFT_COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)
endif()