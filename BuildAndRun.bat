@echo off
setlocal

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" (
    set "TARGET_NAME=SampleApp"
    echo No target name specified; Building %TARGET_NAME% by default
)

set "BUILD_TYPE=Debug"
set "CURRENT_DIR=%CD%"
set "BINARY_OUTPUT_PATH=%CURRENT_DIR%\bin"

echo CURRENT_DIR = %CURRENT_DIR%
echo Building %TARGET_NAME% in %BUILD_TYPE% mode

cmake --build "%CURRENT_DIR%\build" --config "%BUILD_TYPE%" --target "%TARGET_NAME%" -j 12
if errorlevel 1 exit /b %errorlevel%

echo Running %BINARY_OUTPUT_PATH%\%TARGET_NAME%.exe

pushd "%BINARY_OUTPUT_PATH%" || exit /b 1
call "%TARGET_NAME%.exe"
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%