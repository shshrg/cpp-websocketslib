include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")
include_directories(${ASIO_INCLUDE_DIR}
                    ${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_BINARY_DIR}/_deps/sha1)



add_executable(server_test server_test.cpp)
target_link_libraries(server_test PRIVATE GTest::gtest_main my_http_lib)
gtest_discover_tests(server_test)


