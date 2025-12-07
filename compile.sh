#!/bin/bash
set -eu

TYPE=Debug
DIR=build-debug
USE_OPENSSL=ON
BUILD_EXAMPLES=ON
BUILD_BENCHMARKS=OFF

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
        --no-examples)
            BUILD_EXAMPLES=OFF
            ;;
        --bench)
            BUILD_BENCHMARKS=ON
            ;;
        --no-bench)
            BUILD_BENCHMARKS=OFF
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
            echo "  --no-examples       Do not build examples"
            echo "  --bench             Build benchmarks"
            echo "  --no-bench          Do not build benchmarks (default)"
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
cmake -S . -B "$DIR" \
    -DCMAKE_BUILD_TYPE="$TYPE" \
    -DUSE_OPENSSL="$USE_OPENSSL" \
    -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
    -DBUILD_BENCHMARKS="$BUILD_BENCHMARKS"

cmake --build "$DIR" -j"$(nproc)"
