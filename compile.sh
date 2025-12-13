#!/bin/bash
set -eu

TYPE=Debug
DIR=build-debug
USE_OPENSSL=ON
BUILD_EXAMPLES_SERVER=ON
BUILD_EXAMPLES_CLIENT=ON
BUILD_BENCHMARKS=OFF
BUILD_BENCHMARKS_WS=OFF
ENABLE_TESTS=ON

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
            BUILD_EXAMPLES_SERVER=OFF
            BUILD_EXAMPLES_CLIENT=OFF
            ;;
        --no-examples-server)
            BUILD_EXAMPLES_SERVER=OFF
            ;;
        --no-examples-client)
            BUILD_EXAMPLES_CLIENT=OFF
            ;;
        --bench)
            BUILD_BENCHMARKS=ON
            BUILD_BENCHMARKS_WS=ON
            ;;
        --no-bench)
            BUILD_BENCHMARKS=OFF
            BUILD_BENCHMARKS_WS=OFF
            ;;
        --tests)
            ENABLE_TESTS=ON
            ;;
        --no-tests)
            ENABLE_TESTS=OFF
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
            echo "  --no-examples-serv       Do not build server examples"
            echo "  --no-examples-cl       Do not build client examples"
            echo "  --bench             Build benchmarks"
            echo "  --no-bench          Do not build benchmarks (default)"
            echo "  --tests             Build tests (default)"
            echo "  --no-tests          Do not build tests"
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
    -DBUILD_EXAMPLES_SERVER="$BUILD_EXAMPLES_SERVER" \
    -DBUILD_EXAMPLES_CLIENT="$BUILD_EXAMPLES_CLIENT" \
    -DBUILD_BENCHMARKS="$BUILD_BENCHMARKS" \
    -DBUILD_BENCHMARKS_WS="$BUILD_BENCHMARKS_WS" \
    -DENABLE_TESTS=$([ "$ENABLE_TESTS" = "ON" ] && echo ON || echo OFF)

cmake --build "$DIR" -j"$(nproc)"
