add_library(uwebsockets INTERFACE)

target_include_directories(uwebsockets INTERFACE
        ${CMAKE_SOURCE_DIR}/external/uWebSockets/src
        ${CMAKE_SOURCE_DIR}/external/uWebSockets/uSockets/src
)

find_library(USOCKETS_LIB
        NAMES uSockets uSockets.a
        PATHS
        ${CMAKE_SOURCE_DIR}/external/uWebSockets/uSockets
        ${CMAKE_SOURCE_DIR}/external/uWebSockets/uSockets/src
        /usr/local/lib
        /usr/lib
        NO_DEFAULT_PATH
)

if (NOT USOCKETS_LIB)
    message(FATAL_ERROR "Could not find uSockets.a – build uWebSockets with 'WITH_OPENSSL=1 make'")
endif()

# 3) Link deps: uSockets + OpenSSL + zlib + pthread
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(uwebsockets INTERFACE
        ${USOCKETS_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        pthread
)

# Your own WS echo benchmark
add_executable(bench_myserver_ws
        bench_myserver_ws.cpp
)
target_link_libraries(bench_myserver_ws PRIVATE my_http_lib)
set_target_properties(bench_myserver_ws PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# uWebSockets echo benchmark
add_executable(bench_uws
        bench_uws.cpp
)
target_link_libraries(bench_uws PRIVATE uwebsockets)
set_target_properties(bench_uws PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
