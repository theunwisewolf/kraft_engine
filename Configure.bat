@echo off

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" (
    set BUILD_TYPE=Debug;
    echo "No default build type spcified; Using %BUILD_TYPE% as default"
)

echo "Configuring %TARGET_NAME% in %BUILD_TYPE% mode"

set CURRENT_DIR=%CD%
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release

if %BUILD_TYPE% == Debug (
    set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug
)

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE ^
    "-DCMAKE_C_COMPILER:FILEPATH=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe" ^
    "-DCMAKE_CXX_COMPILER:FILEPATH=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe" ^
    %CMAKE_ARGS% ^
    -S %CURRENT_DIR% -B %CURRENT_DIR%/build ^
    -G "Unix Makefiles"
    REM -T host=x64 -A x64 --no-warn-unused-cli
