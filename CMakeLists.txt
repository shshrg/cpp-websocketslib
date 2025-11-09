cmake_minimum_required(VERSION 3.20)
project(websocketlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

# ===== Find OpenSSL =====
find_package(OpenSSL REQUIRED)

add_library(my_http_lib
        src/server.cpp
        src/http/request.cpp
        src/http/response.cpp
        include/websocket/WsFrame.h
)

target_include_directories(my_http_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${ASIO_INCLUDE_DIR}
)
# TODO: no openssl
target_link_libraries(my_http_lib
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(UNIX)
    target_link_libraries(my_http_lib PRIVATE pthread)
    find_library(LIBURING_LIBRARY NAMES uring)
    if (LIBURING_LIBRARY)
        target_link_libraries(my_http_lib PRIVATE "${LIBURING_LIBRARY}")
        target_compile_definitions(my_http_lib PUBLIC ASIO_HAS_IO_URING HAVE_ASIO_FILE_IO)
    endif ()
endif()

# --- Executable server_app
add_executable(server_app src/main.cpp)
target_link_libraries(server_app PRIVATE my_http_lib)
set_target_properties(server_app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# --- Executable client
add_executable(client src/client.cpp src/http/request.cpp)
target_link_libraries(client PRIVATE my_http_lib)
set_target_properties(client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

