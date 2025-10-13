#!/bin/bash

BUILD_DIR=build

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

cmake ..
# shellcheck disable=SC2046
cmake --build . -- -j$(nproc)
cd ..