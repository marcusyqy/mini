@echo off
setlocal
cd /D "%~dp0"

for %%a in (%*) do set "%%a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%clean%" == "1" rd /s /q build && echo [CLEANING SPDLOG]

set debug_flags= /Od /D_DEBUG /MTd
set release_flags= /O2 /DNDEBUG /MT

set compile_flags=
set common_flags= /I..\include\ /nologo /MP /FC /Zi /DSPDLOG_COMPILED_LIB /EHsc

set link_flags= /IGNORE:4006
if "%debug%"=="1" set compile_flags= %debug_flags% %common_flags%
if "%release%"=="1" set compile_flags= %release_flags% %common_flags%

if not exist build mkdir build
pushd build
call cl %compile_flags% -c ..\src\async.cpp
call cl %compile_flags% -c ..\src\bundled_fmtlib_format.cpp
call cl %compile_flags% -c ..\src\cfg.cpp
call cl %compile_flags% -c ..\src\color_sinks.cpp
call cl %compile_flags% -c ..\src\file_sinks.cpp
call cl %compile_flags% -c ..\src\spdlog.cpp
call cl %compile_flags% -c ..\src\stdout_sinks.cpp
call lib *.obj %link_flags% /OUT:spdlog.lib
popd

for %%a in (%*) do set "%%a=0"
set debug_flags=
set release_flags=
set compile_flags=
set common_flags=


