@echo off
setlocal enabledelayedexpansion

set build_type=%1
if "%build_type%"=="" set build_type=Debug

echo Building shader_compiler [%build_type%]

:: ---- Paths ----
set tool_dir=%~dp0
:: Resolve project root (two levels up from tool_dir)
for %%i in ("%tool_dir%..\..") do set root=%%~fi\
set kraft_src=%root%kraft\src
set kraft_vendor=%root%kraft\vendor
set out_dir=%root%bin\%build_type%

if not exist "%out_dir%" mkdir "%out_dir%"

:: ---- Compiler ----
where clang-cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if "%DevEnvDir%"=="" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    )
)

set cc=clang-cl
set vulkan_inc=%VULKAN_SDK%\Include

:: ---- Common flags ----
set arch=--target=x86_64-pc-windows-msvc
set std=/std:c++20 /EHsc
set warnings=/W3

set includes=^
 /I"%kraft_src%"^
 /I"%root%res"^
 /I"%kraft_vendor%"^
 /I"%kraft_vendor%\ufbx\include"^
 /I"%kraft_vendor%\imgui"^
 /I"%kraft_vendor%\glad\include"^
 /I"%kraft_vendor%\glfw\include"^
 /I"%kraft_vendor%\entt\include"^
 /I"%vulkan_inc%"^
 /I"%tool_dir%."

set defines=^
 /DKRAFT_STATIC^
 /DKRAFT_GUI_APP^
 /DVK_NO_PROTOTYPES^
 /DIMGUI_IMPL_VULKAN_USE_VOLK^
 /DIMGUI_DEFINE_MATH_OPERATORS^
 /DGLFW_INCLUDE_NONE^
 /D_ENABLE_EXTENDED_ALIGNED_STORAGE^
 /D_CRT_SECURE_NO_WARNINGS

if /I "%build_type%"=="Debug" (
    set opt=/Od /Zi
    set link_flags=/DEBUG
) else (
    set opt=/O2 /DNDEBUG -flto-visibility-public-std
    set link_flags=/OPT:REF /OPT:ICF
)

:: Vulkan SDK's shaderc_combined.lib is built with /MD (MD_DynamicRelease, _ITERATOR_DEBUG_LEVEL=0)
:: so we must compile everything with /MD to match, and always use the release lib
set common_flags=%arch% %std% %warnings% %includes% %defines% %opt% /MD

:: shaderc from Vulkan SDK (always release variant - the debug lib has the same _ITERATOR_DEBUG_LEVEL=0)
set shaderc_lib=%VULKAN_SDK%\Lib\shaderc_combined.lib

set glfw_lib=%kraft_vendor%\prebuilt\glfw\windows-clang-msvc-x64-debug\glfw3.lib
if /I "%build_type%"=="Release" (
    set glfw_lib=%kraft_vendor%\prebuilt\glfw\windows-clang-msvc-x64-release\glfw3.lib
)

set /a t_build_start=%time:~0,2%*3600+1%time:~3,2%*60+1%time:~6,2%-366
for /f "tokens=2 delims=." %%a in ("%time%") do set t_build_start_cs=%%a

:: ---- Compile ----
call :timer_start
echo [1/4] Compiling kraft.cpp...
%cc% %common_flags% /c "%kraft_src%\kraft.cpp" /Fo"%out_dir%\kraft.obj" /Fd"%out_dir%\kraft.pdb"
if %ERRORLEVEL% neq 0 goto :error
call :timer_end "kraft.cpp"

call :timer_start
echo [2/4] Compiling ufbx.c...
%cc% %arch% /MD /c /I"%kraft_vendor%\ufbx\include\ufbx" "%kraft_vendor%\ufbx\src\ufbx.c" /Fo"%out_dir%\ufbx.obj"
if %ERRORLEVEL% neq 0 goto :error
call :timer_end "ufbx.c"

call :timer_start
echo [3/4] Compiling shader_compiler...
%cc% %common_flags% /c "%tool_dir%vendor\spirv-reflect\spirv_reflect.c" /Fo"%out_dir%\spirv_reflect.obj"
if %ERRORLEVEL% neq 0 goto :error
%cc% %common_flags% /c "%tool_dir%src\main.cpp" /Fo"%out_dir%\shader_compiler.obj" /Fd"%out_dir%\shader_compiler.pdb"
if %ERRORLEVEL% neq 0 goto :error
call :timer_end "shader_compiler"

:: ---- Lib + Link ----
call :timer_start
echo [4/4] Linking shader_compiler.exe...
llvm-lib /OUT:"%out_dir%\kraft.lib" "%out_dir%\kraft.obj" "%out_dir%\ufbx.obj"
if %ERRORLEVEL% neq 0 goto :error

set system_libs=user32.lib gdi32.lib shell32.lib kernel32.lib ole32.lib ucrt.lib msvcrt.lib

lld-link /OUT:"%out_dir%\shader_compiler.exe"^
 "%out_dir%\shader_compiler.obj"^
 "%out_dir%\spirv_reflect.obj"^
 "%out_dir%\kraft.lib"^
 "%shaderc_lib%"^
 "%glfw_lib%"^
 %system_libs%^
 %link_flags% /SUBSYSTEM:CONSOLE
if %ERRORLEVEL% neq 0 goto :error
call :timer_end "link"

:: Total time
set /a t_build_end=%time:~0,2%*3600+1%time:~3,2%*60+1%time:~6,2%-366
for /f "tokens=2 delims=." %%a in ("%time%") do set t_build_end_cs=%%a
set /a t_total_s=t_build_end-t_build_start
set /a t_total_cs=t_build_end_cs-t_build_start_cs
if !t_total_cs! lss 0 (
    set /a t_total_s-=1
    set /a t_total_cs+=100
)
if !t_total_cs! lss 10 set t_total_cs=0!t_total_cs!

echo.
echo Build successful: %out_dir%\shader_compiler.exe
echo Total: %t_total_s%.%t_total_cs%s
goto :end

:error
echo.
echo Build FAILED.
exit /b 1

:timer_start
set /a t_start=%time:~0,2%*3600+1%time:~3,2%*60+1%time:~6,2%-366
for /f "tokens=2 delims=." %%a in ("%time%") do set t_start_cs=%%a
exit /b

:timer_end
set /a t_end=%time:~0,2%*3600+1%time:~3,2%*60+1%time:~6,2%-366
for /f "tokens=2 delims=." %%a in ("%time%") do set t_end_cs=%%a
set /a t_elapsed_s=t_end-t_start
set /a t_elapsed_cs=t_end_cs-t_start_cs
if !t_elapsed_cs! lss 0 (
    set /a t_elapsed_s-=1
    set /a t_elapsed_cs+=100
)
if !t_elapsed_cs! lss 10 set t_elapsed_cs=0!t_elapsed_cs!
echo   %~1: %t_elapsed_s%.%t_elapsed_cs%s
exit /b

:end
endlocal
