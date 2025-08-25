#pragma once

#include "kraft_filesystem_types.h"
#include "kraft_filesystem.h"
#include "kraft_platform.h"
#include "kraft_window_types.h"
#include "kraft_window.h"

#if defined(KRAFT_PLATFORM_WINDOWS)
#include "Windows.h"
#include "windows/kraft_win32_types.h"
#include "windows/kraft_win32.h"
#elif defined(KRAFT_PLATFORM_LINUX)
#include "linux/kraft_linux_types.h"
#include "linux/kraft_linux.h"
#elif defined(KRAFT_PLATFORM_MACOS)
#include "macos/kraft_macos_types.h"
#include "macos/kraft_macos.h"
#endif