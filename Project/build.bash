#!/bin/bash

if [ ! -d ../Build ]; then
  mkdir ../Build
fi

pushd ../Build

ls


# TODO: problems with linkage. Maybe i just need the correct version of LLVM or something
# also suppress the 100000 warnings
clang -I ../Project/Source -o Constellate ../Project/Source/unity_build.cpp

popd
