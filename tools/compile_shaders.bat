@echo off

echo "Compiling shaders..."

set src=%1
set dst=%2
shift
shift

for /r %src% %%f in (*.vert) do (
    echo "Input: %%f"
    echo "Output: %dst%/shaders/%%~nf%%~xf.spv"
    glslc -o "%dst%/shaders/%%~nf%%~xf.spv" %%f 
)

for /r %src% %%f in (*.frag) do (
    echo "Input: %%f"
    echo "Output: %dst%/shaders/%%~nf%%~xf.spv"
    glslc -o "%dst%/shaders/%%~nf%%~xf.spv" %%f 
)