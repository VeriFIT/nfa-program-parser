#!/bin/bash

BASEDIR=$(realpath $(dirname "$0"))

echo "[!] Building Python interpret"

make -C "$BASEDIR/src/python" install
if [ $? -ne 0 ]; then
  exit 1
fi
