# examples/CMakeLists.txt

add_executable(example_http http_basic.cpp)
target_link_libraries(example_http PRIVATE my_http_lib)

target_compile_definitions(example_http PRIVATE
        DEFAULT_CONFIG_PATH="${CMAKE_SOURCE_DIR}/examples/configs/http_basic.conf"
)

add_executable(example_ws ws_basic.cpp)
target_link_libraries(example_ws PRIVATE my_http_lib)

target_compile_definitions(example_ws PRIVATE
        DEFAULT_CONFIG_WS_PATH="${CMAKE_SOURCE_DIR}/examples/configs/ws_basic.conf"
)

set_target_properties(example_http example_ws PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)