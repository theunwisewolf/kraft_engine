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


add_subdirectory(${KRAFT_PATH}/vendor/volk ${KRAFT_BINARY_DIR}/vendor/volk)
# add_subdirectory(${KRAFT_PATH}/vendor/glfw ${KRAFT_BINARY_DIR}/vendor/glfw)

# set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
# set(BUILD_SHARED_LIBS OFF)
# add_subdirectory(${KRAFT_PATH}/vendor/assimp ${KRAFT_BINARY_DIR}/vendor/assimp)
# set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")

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
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(KRAFT_LIBRARY_COMPILER_PREFIX "clang-msvc")
    elseif (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        set(KRAFT_LIBRARY_COMPILER_PREFIX "clang-gnu")
    else()
        set(KRAFT_LIBRARY_COMPILER_PREFIX "clang")
    endif()

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

set(KRAFT_PREBUILT_LIB_PATH "${KRAFT_PATH}/vendor/prebuilt")
set(KRAFT_PREBUILT_LIB_FOLDER "${KRAFT_LIBRARY_OS_PREFIX}-${KRAFT_LIBRARY_COMPILER_PREFIX}-${KRAFT_LIBRARY_ARCH_PREFIX}-${KRAFT_LIBRARY_BUILD_TYPE_PREFIX}")
message(${KRAFT_PREBUILT_LIB_FOLDER})
# set(KRAFT_STATIC_LIBS assimp::assimp)
set(KRAFT_STATIC_LIBS)
list(APPEND KRAFT_STATIC_LIBS ${KRAFT_PREBUILT_LIB_PATH}/assimp/${KRAFT_PREBUILT_LIB_FOLDER}/assimp.lib)
# list(APPEND KRAFT_STATIC_LIBS ${KRAFT_PREBUILT_LIB_PATH}/assimp/windows-clang-x64-release/assimp.lib)
list(APPEND KRAFT_STATIC_LIBS ${KRAFT_PREBUILT_LIB_PATH}/zlib/${KRAFT_PREBUILT_LIB_FOLDER}/zlibstatic.lib)

set(KRAFT_STATIC_LIBS_INCLUDE_PATHS vendor vendor/assimp/include)
list(APPEND KRAFT_STATIC_LIBS_INCLUDE_PATHS ${KRAFT_PREBUILT_LIB_PATH}/assimp/${KRAFT_PREBUILT_LIB_FOLDER}/include)
# list(APPEND KRAFT_STATIC_LIBS_INCLUDE_PATHS ${KRAFT_PREBUILT_LIB_PATH}/assimp/windows-clang-x64-release/include)

set(KRAFT_STATIC_LIBS_COMPILE_DEFINITIONS)
set(KRAFT_COMPILE_DEFINITIONS _ENABLE_EXTENDED_ALIGNED_STORAGE)

if (KRAFT_APP_TYPE STREQUAL "GUI")
    add_subdirectory(${KRAFT_PATH}/vendor/ufbx)

    # Include paths
    list(APPEND KRAFT_STATIC_LIBS_INCLUDE_PATHS 
        ${KRAFT_PATH}/vendor/imgui 
        ${KRAFT_PATH}/vendor/glad/include 
        ${KRAFT_PATH}/vendor/glfw/include 
        ${KRAFT_PATH}/vendor/entt/include 
        ${KRAFT_PATH}/vendor/ufbx/include 
    )

    # Compile-time definitions
    list(APPEND KRAFT_STATIC_LIBS_COMPILE_DEFINITIONS GLFW_INCLUDE_NONE)

    # Libraries
    # list(APPEND KRAFT_STATIC_LIBS glfw ${GLFW_LIBRARIES} ${Vulkan_LIBRARY})
    list(APPEND KRAFT_STATIC_LIBS volk::volk_headers Ufbx)
    list(APPEND KRAFT_STATIC_LIBS ${KRAFT_PREBUILT_LIB_PATH}/glfw/${KRAFT_PREBUILT_LIB_FOLDER}/glfw3.lib)

    # Kraft compile time definitions
    list(APPEND KRAFT_COMPILE_DEFINITIONS KRAFT_GUI_APP VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)
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