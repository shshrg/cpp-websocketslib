#!/bin/bash
set -eu

TYPE=Debug
DIR=build-debug
USE_OPENSSL=ON

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--debug)
            TYPE=Debug
            DIR=build-debug
            ;;
        -o|--release)
            TYPE=Release
            DIR=build-release
            ;;
        --openssl)
            USE_OPENSSL=ON
            ;;
        --no-openssl)
            USE_OPENSSL=OFF
            ;;
        -c|--clean)
            rm -rf build-*
            exit 0
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  -d, --debug         Build Debug (default)"
            echo "  -o, --release       Build Release"
            echo "  --openssl           Build with OpenSSL (default)"
            echo "  --no-openssl        Build without OpenSSL"
            echo "  -c, --clean         Remove build directories"
            echo "  -h, --help          Show this help"
            exit 0
            ;;
        *)
            echo "Unknown arg: $1"
            exit 1
            ;;
    esac
    shift
done

mkdir -p "$DIR"
cmake -S . -B "$DIR" -DCMAKE_BUILD_TYPE="$TYPE" -DUSE_OPENSSL="$USE_OPENSSL"
cmake --build "$DIR" -j"$(nproc)"
