#!/bin/sh

OLDPWD=$PWD
cd "${0%/*}" # cd to where the script is

COMPILER=cc
CALL=sh

# declare variables
debug=0
release=0

[ ! -d "build" ] && mkdir build

# set variables to
for arg in "$@"; do
    eval "$arg=1"
done

echo COMPILER:$COMPILER

COMMON_FLAGS="-g"
DEBUG_FLAGS="-O0 -D_DEBUG"
RELEASE_FLAGS="-O2"

if [ "$release" -eq "1" ]; then
   echo "Release enabled"
   eval "TARGET_FLAGS=\"$RELEASE_FLAGS\""
else
    eval "debug=1"
fi

[ "$debug" -eq "1" ] && echo "Debug enabled" && eval "TARGET_FLAGS=\"$DEBUG_FLAGS\""

cd build
$COMPILER $COMMON_FLAGS $TARGET_FLAGS -c -D_GLFW_X11 ../glfw_common.c -o glfw_common.o &
$COMPILER $COMMON_FLAGS $TARGET_FLAGS -c -D_GLFW_X11 ../glfw_x11.c -o glfw_x11.o  &
wait
ar rvs glfw.a glfw_common.o glfw_x11.o

# end with reverting back
cd $OLDPWD

