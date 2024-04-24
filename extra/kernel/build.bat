@echo off
setlocal
cd /D "%~dp0"

set COMPILER= %VULKAN_SDK%\Bin\glslangValidator.exe

:: In the future we should look into each file.
if not exist build mkdir build
pushd build
%COMPILER% -V ..\gradient.comp -o gradient.comp.spv
popd
