# benchmarks/CMakeLists.txt

include(FetchContent)

FetchContent_Declare(
        cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
)

FetchContent_MakeAvailable(cpp_httplib)

# --- myserver benchmark ---

# ==== Hello ====
add_executable(bench_myserver_http
        bench_myserver.cpp
)
target_link_libraries(bench_myserver_http PRIVATE my_http_lib)
set_target_properties(bench_myserver_http PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
target_compile_definitions(bench_myserver_http PRIVATE
        DEFAULT_CONFIG_WS_PATH="${CMAKE_SOURCE_DIR}/examples/configs/ws_basic.conf"
)


# --- cpp-httplib benchmark ---

# ==== Hello ====
add_executable(bench_httplib
        bench_httplib.cpp
)
target_include_directories(bench_httplib PRIVATE
        ${cpp_httplib_SOURCE_DIR}
)

set_target_properties(bench_httplib PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)