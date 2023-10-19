#!/bin/bash
basedir=$(realpath $(dirname "$0"))
echo "[!] Building Python interpret"
make -C "$basedir/src/python" install
