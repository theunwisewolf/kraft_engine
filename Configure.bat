@echo off

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" (
    set BUILD_TYPE=Debug
    echo "No build type specified; Using Debug as default"
)

set ARCH_TYPE=%2
if "%ARCH_TYPE%"=="" (
    set ARCH_TYPE=x64
    echo "No arch type specified; Using x64 as default"
)

echo "Configuring %TARGET_NAME% in %BUILD_TYPE% mode (%ARCH_TYPE%)"

set CURRENT_DIR=%CD%
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release

if %BUILD_TYPE% == Debug (
    set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug
)

REM Set Visual Studio vars
if "%DevEnvDir%" == "" (
	call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" %ARCH_TYPE%
)

set CMAKE_PATH="%DevEnvDir%CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE_PATH% --version
%CMAKE_PATH% -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE ^
    "-DCMAKE_C_COMPILER:FILEPATH=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe" ^
    "-DCMAKE_CXX_COMPILER:FILEPATH=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe" ^
    %CMAKE_ARGS% ^
    -S %CURRENT_DIR% -B %CURRENT_DIR%/build ^
    -G "Ninja"
    REM -T host=x64 -A x64 --no-warn-unused-cli