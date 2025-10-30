#!/bin/bash
set -euo pipefail

TYPE=Debug
DIR=build-debug

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--debug) TYPE=Debug; DIR=build-debug ;;
        -o|--release) TYPE=Release; DIR=build-release ;;
        -c|--clean) rm -rf build-*; exit 0 ;;
        -h|--help) echo "Usage: $0 [-d|--debug] [-o|-release] [-c|--clean]"; exit 0 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
    shift
done

mkdir -p "$DIR"
cmake -S . -B "$DIR" -DCMAKE_BUILD_TYPE="$TYPE"
cmake --build "$DIR" -j"$(nproc)"