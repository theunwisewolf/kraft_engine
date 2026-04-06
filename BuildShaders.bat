@echo off
setlocal enabledelayedexpansion

set build_type=%1
if "%build_type%"=="" set build_type=Debug

set shader_dir=%2
:: Strip trailing slashes — a trailing \ before a closing " causes
:: MSVC CRT to parse \" as a literal quote, corrupting argv
:strip_slash
if "!shader_dir:~-1!"=="\" set "shader_dir=!shader_dir:~0,-1!" & goto :strip_slash
if "!shader_dir:~-1!"=="/" set "shader_dir=!shader_dir:~0,-1!" & goto :strip_slash
if "%shader_dir%"=="" (
    echo Usage: BuildShaders.bat [Debug^|Release] ^<shader_directory^>
    echo Example: BuildShaders.bat Debug text_drawing/res/shaders
    exit /b 1
)

set root=%~dp0
set compiler_exe=%root%bin\%build_type%\shader_compiler.exe

echo Building shader compiler [%build_type%]...
call "%root%tools\shader_compiler\Build.bat" %build_type%
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo Compiling shaders in %shader_dir%...
"%compiler_exe%" "%root%%shader_dir%"
if %ERRORLEVEL% neq 0 (
    echo Shader compilation failed.
    exit /b 1
)

echo Done.
endlocal
