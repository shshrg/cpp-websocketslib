cmake_minimum_required(VERSION 4.0)
project(websocketlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-36-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

add_executable(server
        src/http/request.cpp
        src/http/response.cpp
        src/server.cpp)

add_executable(client src/client.cpp src/http/request.cpp)

if(UNIX)
    target_link_libraries(server PRIVATE pthread)
endif()


target_include_directories(client PRIVATE ${asio_SOURCE_DIR}/asio/include ${CMAKE_SOURCE_DIR}/include)
target_include_directories(server PRIVATE ${asio_SOURCE_DIR}/asio/include ${CMAKE_SOURCE_DIR}/include)

