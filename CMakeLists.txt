cmake_minimum_required(VERSION 3.20)
project(websocketlib LANGUAGES CXX)

if (WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

option(BUILD_EXAMPLES_SERVER "Build HTTP/WS server examples" ON)
option(BUILD_EXAMPLES_CLIENT "Build HTTP/WS client examples" ON)
option(BUILD_BENCHMARKS "Build benchmark executables" OFF)
option(BUILD_BENCHMARKS_WS "Build WebSocket benchmarks" OFF)
option(ENABLE_TESTS "Build and run unit tests" ON)

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

# ===== Hash =====
set(SHA1_URL "https://raw.githubusercontent.com/Chocobo1/Hash/master/src/sha1.h")
set(SHA1_HEADER "${CMAKE_BINARY_DIR}/_deps/sha1/sha1.h")
file(DOWNLOAD "${SHA1_URL}" "${SHA1_HEADER}" SHOW_PROGRESS)

# ===== OpenSSL =====
option(USE_OPENSSL "Enable SSL support" ON)

if(USE_OPENSSL)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
        add_compile_definitions(USE_SSL)
        set(USE_SSL_ENABLED TRUE)
    else()
        message(WARNING "OpenSSL NOT found -> building WITHOUT SSL")
    endif()
else()
    set(USE_SSL_ENABLED FALSE)
endif()

# ==== Main Library ====
add_library(my_http_lib
        src/server.cpp
        src/http/request.cpp
        src/http/response.cpp
        include/websocket/WsFrame.h
        src/config.cpp
        src/http_client.cpp
        src/websocket_client.cpp
)

target_include_directories(my_http_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${ASIO_INCLUDE_DIR}
        ${CMAKE_BINARY_DIR}/_deps/sha1
)

if(USE_SSL_ENABLED)
    target_link_libraries(my_http_lib
            PUBLIC
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()

if (WIN32)
    target_link_libraries(my_http_lib
            PUBLIC
            ws2_32
            mswsock
    )
endif ()

if (UNIX)
    target_link_libraries(my_http_lib PRIVATE pthread)
    find_library(LIBURING_LIBRARY NAMES uring)
    if (LIBURING_LIBRARY)
        target_link_libraries(my_http_lib PRIVATE "${LIBURING_LIBRARY}")
        target_compile_definitions(my_http_lib PUBLIC ASIO_HAS_IO_URING HAVE_ASIO_FILE_IO)
    endif ()
endif ()

add_executable(server_app src/main.cpp)
target_link_libraries(server_app PRIVATE my_http_lib)
set_target_properties(server_app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

if(BUILD_EXAMPLES_SERVER)
    add_subdirectory(examples/servers)
endif()

if(BUILD_EXAMPLES_CLIENT)
    add_subdirectory(examples/clients)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks/http)
endif()

if (BUILD_BENCHMARKS_WS)
    add_subdirectory(benchmarks/ws)
endif()

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()