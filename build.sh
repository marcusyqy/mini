#!/bin/sh
OLDPWD=$PWD
cd "${0%/*}" # cd to where the script is

COMPILER=c++
PLATFORM=$(uname -s)
TARGET=DEBUG

COMMON_FLAGS="-g -std=c++17 -Wall -Werror -Wpedantic" # -Wsign-conversion"
DEBUG_FLAGS="-O0 -D_DEBUG"
RELEASE_FLAGS="-O2"
INCLUDE_FLAGS="-I../extra/imgui/imgui -I../extra/glfw/include -I../extra/minigraph -IX11"
LINK_FLAGS="-L$VULKAN_SDK/lib ../extra/glfw/build/glfw.a ../extra/imgui/build/imgui.a ../extra/adapter/build/adapter.a -lGL -lX11 -ldl -pthread -lvulkan-1 -lshaderc_shared"

# declare variables
debug=0
release=0
format=0
glfw=0
imgui=0
main=0
all=0
run=0
adapter=0

if [ -z "$*" ]; then
    echo NO ARGUMENTS. DEFAULTING TO BUILD ALL.
    all=1
fi

[ ! -d "build" ] && mkdir build

# set variables to
for arg in "$@"; do
    eval "$arg=1"
done

if [ "$release" -eq "1" ]; then
   eval "TARGET_FLAGS=\"$RELEASE_FLAGS\""
   eval "TARGET=DEBUG"
else
    eval "debug=1"
fi

[ "$debug" -eq "1" ] && eval "TARGET=DEBUG" && eval "TARGET_FLAGS=\"$DEBUG_FLAGS\""


echo COMPILER:$COMPILER
echo TARGET:$TARGET
echo PLATFORM:$PLATFORM

if [ "$all" -eq "1" ]; then
    eval "imgui=1"
    eval "main=1"
    eval "glfw=1"
    eval "adapter=1"
fi

if [ "$glfw" -eq "1" ]; then
    echo "BUILD glfw"
    sh "extra/glfw/build.sh" "$@"
fi

if [ "$imgui" -eq "1" ]; then
    echo "BUILD IMGUI"
    sh "extra/imgui/build.sh" "$@"
fi

if [ "$adapter" -eq "1" ]; then
    echo "BUILD ADAPTER"
    sh "extra/adapter/build.sh" "$@"
fi


# TODO:
cd build
if [ "$main" -eq "1" ]; then
    echo "BUILD MINI"
    $COMPILER $COMMON_FLAGS $TARGET_FLAGS $INCLUDE_FLAGS ../mini/single_include_compile.cpp  $LINK_FLAGS -o mini
fi
if [ "$run" -eq "1" ]; then
    echo "RUNNING MINI"
    ./mini
fi


# end with reverting back
cd $OLDPWD
