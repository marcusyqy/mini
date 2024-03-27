#!/bin/sh

OLDPWD=$PWD
cd "${0%/*}" # cd to where the script is

COMPILER=c++
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
$COMPILER $COMMON_FLAGS $TARGET_FLAGS -I../imgui/ -c ../imgui_lib.cpp -o imgui_lib.o
ar rvs imgui.a imgui_lib.o

# end with reverting back
cd $OLDPWD


