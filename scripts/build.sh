#!/bin/bash
unamestr=`uname`
if [[ "$unamestr" == 'Darwin' ]]; then
	whitgl_ninja='input/ninja/ninja'
	ninja='whitgl/input/ninja/ninja'
elif [[ "$unamestr" == MINGW32_NT* ]]; then
	whitgl_ninja='input/ninja.exe'
	ninja='whitgl/input/ninja.exe'
else
	whitgl_ninja='ninja'
	ninja='ninja'
fi
set -e
pushd whitgl
python build.py "$@"
$whitgl_ninja -f build/build.ninja
popd
