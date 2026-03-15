"""
Generates compile_commands.json for the Kraft unity build.

Walks the #include chain starting from kraft/src/kraft.cpp to find all
individual .cpp files, then generates a compile_commands.json entry for
each one with the correct include paths and defines.

Usage:
    python tools/generate_compile_commands.py [--gui] [--debug]

    --gui       Include GUI-only files and defines (default: GUI mode)
    --console   Console app mode (no renderer/window/world files)
    --debug     Add DEBUG define
"""

import os
import sys
import json
import re
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
KRAFT_SRC = os.path.join(ROOT_DIR, "kraft", "src")
KRAFT_VENDOR = os.path.join(ROOT_DIR, "kraft", "vendor")


def find_vulkan_sdk():
    vk = os.environ.get("VULKAN_SDK") or os.environ.get("VK_SDK_PATH")
    if vk:
        return vk
    # Fallback: check common install location
    base = "C:/VulkanSDK"
    if os.path.isdir(base):
        versions = sorted(os.listdir(base), reverse=True)
        if versions:
            return os.path.join(base, versions[0])
    return None


def collect_cpp_files(includes_cpp_path, is_gui, is_windows):
    """Parse an _includes.cpp file and return list of absolute .cpp paths."""
    results = []
    base_dir = os.path.dirname(includes_cpp_path)

    if not os.path.isfile(includes_cpp_path):
        return results

    with open(includes_cpp_path, "r") as f:
        lines = f.readlines()

    # Track preprocessor conditionals (very basic)
    skip_depth = 0
    for line in lines:
        stripped = line.strip()

        # Handle basic #if / #endif for KRAFT_GUI_APP and KRAFT_PLATFORM_*
        if stripped.startswith("#if"):
            cond = stripped
            if "KRAFT_GUI_APP" in cond and not is_gui:
                skip_depth += 1
                continue
            if "KRAFT_PLATFORM_WINDOWS" in cond and not is_windows:
                skip_depth += 1
                continue
            if "KRAFT_PLATFORM_LINUX" in cond and is_windows:
                skip_depth += 1
                continue
            if "KRAFT_PLATFORM_MACOS" in cond and is_windows:
                skip_depth += 1
                continue
        elif stripped.startswith("#elif"):
            if skip_depth > 0:
                # Check if this elif branch should be active
                if "KRAFT_PLATFORM_WINDOWS" in stripped and is_windows:
                    skip_depth -= 1
                elif "KRAFT_PLATFORM_LINUX" in stripped and not is_windows:
                    skip_depth -= 1
            continue
        elif stripped == "#endif":
            if skip_depth > 0:
                skip_depth -= 1
            continue
        elif stripped.startswith("#else"):
            if skip_depth > 0:
                skip_depth -= 1  # The else branch is active
            else:
                skip_depth += 1  # The else branch is inactive
            continue

        if skip_depth > 0:
            continue

        # Match #include "something.cpp"
        m = re.match(r'#include\s+"([^"]+\.cpp)"', stripped)
        if m:
            included = m.group(1)
            full_path = os.path.normpath(os.path.join(base_dir, included))

            # If it's another _includes.cpp, recurse
            if included.endswith("_includes.cpp"):
                results.extend(collect_cpp_files(full_path, is_gui, is_windows))
            else:
                results.append(full_path)

    return results


def collect_all_unity_files(is_gui, is_windows):
    """Walk kraft.cpp's include chain to find all individual .cpp files."""
    kraft_cpp = os.path.join(KRAFT_SRC, "kraft.cpp")
    results = [kraft_cpp]

    with open(kraft_cpp, "r") as f:
        lines = f.readlines()

    skip_depth = 0
    for line in lines:
        stripped = line.strip()

        if stripped.startswith("#if"):
            if "KRAFT_GUI_APP" in stripped and not is_gui:
                skip_depth += 1
                continue
        elif stripped == "#endif":
            if skip_depth > 0:
                skip_depth -= 1
            continue
        elif stripped.startswith("#else"):
            if skip_depth > 0:
                skip_depth -= 1
            else:
                skip_depth += 1
            continue

        if skip_depth > 0:
            continue

        m = re.match(r'#include\s+<([^>]+_includes\.cpp)>', stripped)
        if m:
            rel_path = m.group(1)
            includes_path = os.path.normpath(os.path.join(KRAFT_SRC, rel_path))
            results.extend(collect_cpp_files(includes_path, is_gui, is_windows))

    return results


def build_compile_commands(is_gui, is_debug, is_windows):
    vulkan_sdk = find_vulkan_sdk()

    # Include paths (relative to kraft/)
    include_dirs = [
        KRAFT_SRC,
        os.path.join(ROOT_DIR, "res"),
        KRAFT_VENDOR,
        os.path.join(KRAFT_VENDOR, "ufbx", "include"),
    ]

    if vulkan_sdk:
        include_dirs.append(os.path.join(vulkan_sdk, "Include"))

    if is_gui:
        include_dirs.extend([
            os.path.join(KRAFT_VENDOR, "imgui"),
            os.path.join(KRAFT_VENDOR, "glad", "include"),
            os.path.join(KRAFT_VENDOR, "glfw", "include"),
            os.path.join(KRAFT_VENDOR, "entt", "include"),
        ])

    # Defines
    defines = ["_ENABLE_EXTENDED_ALIGNED_STORAGE", "KRAFT_STATIC"]
    if is_gui:
        defines.extend(["KRAFT_GUI_APP", "VK_NO_PROTOTYPES", "IMGUI_IMPL_VULKAN_USE_VOLK", "GLFW_INCLUDE_NONE"])
    else:
        defines.append("KRAFT_CONSOLE_APP")
    if is_windows:
        defines.append("_CRT_SECURE_NO_WARNINGS")
        defines.append("KRAFT_PLATFORM_WINDOWS")
    if is_debug:
        defines.extend(["DEBUG", "KRAFT_DEBUG"])

    cpp_files = collect_all_unity_files(is_gui, is_windows)

    # Build the compile command string
    include_flags = " ".join(f'-I"{d}"' for d in include_dirs if os.path.isdir(d))
    define_flags = " ".join(f"-D{d}" for d in defines)

    entries = []
    for cpp_file in cpp_files:
        cpp_file = os.path.normpath(cpp_file)
        command = f"clang-cl.exe /std:c++20 /EHsc {define_flags} {include_flags} /c \"{cpp_file}\""
        entries.append({
            "directory": ROOT_DIR,
            "command": command,
            "file": cpp_file,
        })

    return entries


def main():
    parser = argparse.ArgumentParser(description="Generate compile_commands.json for Kraft unity build")
    parser.add_argument("--gui", action="store_true", default=True, help="GUI app mode (default)")
    parser.add_argument("--console", action="store_true", help="Console app mode")
    parser.add_argument("--debug", action="store_true", default=True, help="Debug build (default)")
    parser.add_argument("--release", action="store_true", help="Release build")
    parser.add_argument("-o", "--output", default=None, help="Output path (default: <root>/compile_commands.json)")
    args = parser.parse_args()

    is_gui = not args.console
    is_debug = not args.release
    is_windows = sys.platform == "win32"

    entries = build_compile_commands(is_gui, is_debug, is_windows)

    output_path = args.output or os.path.join(ROOT_DIR, "compile_commands.json")
    with open(output_path, "w") as f:
        json.dump(entries, f, indent=2)

    print(f"Generated {output_path} with {len(entries)} entries")


if __name__ == "__main__":
    main()
