#!/bin/bash
#
BASEDIR=$(realpath $(dirname "$0"))
PROJECTDIR=$(realpath "$BASEDIR/../..")

echo "[!] Building C++ interpret"
pushd $BASEDIR
mkdir -p build
cd build
cmake ..  -DCMAKE_BUILD_TYPE=Release
make
if [ $? -ne 0 ]; then
  exit 1
fi

cp $BASEDIR/build/src/cpp/*-interpreter $PROJECTDIR/bin
popd
echo "[done]"
