cmake_minimum_required(VERSION 4.0)
project(websocketlib)

set (CMAKE_CXX_STANDARD 20)

add_executable(${CMAKE_PROJECT_NAME} main.cpp)

include_directories(${CMAKE_PROJECT_NAME} include)

add_executable(server_example server_example.cpp)

include_directories(server_example external/libs)