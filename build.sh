#!/bin/bash
basedir=$(realpath $(dirname "$0"))
echo $ROOT
echo "[!] Building C++ interpret"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
if [ $? -ne 0 ]; then
  exit 1
fi

cp ./src/cpp/*-interpreter ~/bin
echo "[done]"

echo "[!] Building Python interpret"
make -C "$basedir/src/python" install
