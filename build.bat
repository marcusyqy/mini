@echo off
setlocal
cd /D "%~dp0"

:: @TODO add clang
for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%~1"==""        echo [default mode] && set main=1

if "%all%" == "1" (
    set glfw=1
    set imgui=1
    set adapter=1
    set main=1
)

if "%clean%" == "1" (
    rd /s /q build && echo [CLEANING MINI]
) 

:: will always setup and let the script take note of when we need to rebuild instead.
call python3 setup.py

:: build glfw
set build_glfw=
if "%glfw%"=="1" set build_glfw= call build && echo [BUILDING GLFW]
pushd extra\glfw
call %build_glfw%
popd

:: build imgui
set build_imgui=
if "%imgui%"=="1" set build_imgui= call build && echo [BUILDING IMGUI]
pushd extra\imgui
%build_imgui%
popd

:: build adapter
set build_adapter=
if "%adapter%"=="1" set build_adapter= call build && echo [BUILDING ADAPTER]
pushd extra\adapter
%build_adapter%
popd

set FILEMASK=*.c,*.cc,*.cpp,*.h,*.hh,*.hpp
if "%format%" == "1" (
echo [clang-format]
pushd mini
    for /R %%f in (%FILEMASK%) do (
        echo ["formatting - %%f"]
        clang-format -i "%%f"
        clang-format -i "%%f"
    )
popd
)

set debug_flags= -Od -D_DEBUG -MTd
set release_flags= -O2 -DNDEBUG -MT
set debug_links="shaderc_sharedd.lib"
set release_links="shaderc_shared.lib"

set compile_flags=
set external_includes_dep= -external:I..\extra\imgui -external:I..\extra\glfw\include -external:I..\extra\adapter -external:I%VULKAN_SDK%\Include -external:I..\extra\volk -external:I..\extra\glm -external:I..\extra\vma 
set include_deps= %external_includes_dep% -I..\mini\ 
set common_flags= %include_deps% -nologo -MP -FC -Zi -Zc:__cplusplus -wd4530 -utf-8 -WX -W3 -external:W0 -EHsc
:: -std:c++17

if "%debug%"=="1" set compile_flags= %debug_flags% %common_flags%
if "%release%"=="1" set compile_flags= %release_flags% %common_flags%

set glfw_link= ..\extra\glfw\build\glfw.lib
set imgui_link= ..\extra\imgui\build\imgui.lib
set adapters_link= ..\extra\adapter\build\adapter.lib

set win32_link=gdi32.lib kernel32.lib user32.lib Shell32.lib legacy_stdio_definitions.lib
set common_links= -link -LIBPATH:%VULKAN_SDK%\lib %imgui_link% %glfw_link% %adapters_link% %win32_link% vulkan-1.lib

set links=
if "%debug%"=="1" set links= %common_links% %debug_links%
if "%release%"=="1" set links= %common_links% %release_links%

if not exist build mkdir build
pushd build
set build_main=
if "%main%"=="1" set build_main= call cl %compile_flags% ..\mini\single_include_compile.cpp -Fe:mini.exe %links% && echo [BUILDING MINI]
%build_main% 
set run_main=
if "%run%" == "1" set run_main= call mini && echo [RUNNING MINI]
%run_main%
popd

for %%a in (%*) do set "%%a=0"
set debug=
set release=
set compile_flags=
set common_flags=

