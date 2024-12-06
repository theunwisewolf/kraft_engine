@echo off

set TARGET_NAME=%1
if "%TARGET_NAME%"=="" (
    set TARGET_NAME=KraftEditor
    echo "No target name specified; Building %TARGET_NAME% by default"
)

set BUILD_TYPE=Debug
set CURRENT_DIR=%CD%

echo "Building %TARGET_NAME% in %BUILD_TYPE% mode"

cmake --build %CURRENT_DIR%/build --config %BUILD_TYPE% ^
    --target %TARGET_NAME% -j 12 -- ^
    && %CURRENT_DIR%/bin/%BUILD_TYPE%/%TARGET_NAME%.exe
