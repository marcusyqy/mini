@echo off
setlocal
cd /D "%~dp0"

for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

set debug_flags= /Od /D_DEBUG /MTd
set release_flags= /O2 /DNDEBUG /MT

set compile_flags=
set common_flags= /I..\..\imgui\imgui /I..\..\glfw\include -I%VULKAN_SDK%\Include /nologo /MP /FC /Zi /D_GLFW_WIN32

set link_flags= /IGNORE:4006
if "%debug%"=="1" set compile_flags= %debug_flags% %common_flags%
if "%release%"=="1" set compile_flags= %release_flags% %common_flags%

set links= gdi32.lib shell32.lib

if not exist build mkdir build
pushd build
echo cl %compile_flags% -c ..\*.cpp 
call cl %compile_flags% -c ..\*.cpp 
call lib %links% *.obj %link_flags% /OUT:adapter.lib
popd

for %%a in (%*) do set "%%a=0"
set debug_flags=
set release_flags=
set compile_flags=
set common_flags=


