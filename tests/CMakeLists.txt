# tests/CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

set(TEST_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/_deps/sha1
        ${ASIO_INCLUDE_DIR}
)

# --- HTTP Server tests ---
add_executable(http_test http_server_test.cpp)
target_include_directories(http_test PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(http_test PRIVATE my_http_lib gtest_main)

gtest_discover_tests(http_test)

# --- WebSocket Server tests ---
add_executable(ws_test ws_server_test.cpp)
target_include_directories(ws_test PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(ws_test PRIVATE my_http_lib gtest_main)

gtest_discover_tests(ws_test)