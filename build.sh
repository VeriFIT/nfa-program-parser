#!/bin/bash
basedir=$(realpath $(dirname "$0"))
echo "[!] Building C++ interpret"
pushd $basedir
mkdir -p build
cd build
cmake ..  -DCMAKE_BUILD_TYPE=Release
make
if [ $? -ne 0 ]; then
  exit 1
fi

cp ./src/cpp/*-interpreter ~/bin
popd
echo "[done]"
