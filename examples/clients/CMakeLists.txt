# examples/CMakeLists.txt

add_executable(example_http_client http_client_example.cpp)
target_link_libraries(example_http_client PRIVATE my_http_lib)

target_compile_definitions(example_http_client PRIVATE
        DEFAULT_CONFIG_PATH="${CMAKE_SOURCE_DIR}/examples/configs/http_basic.conf"
)

add_executable(example_ws_client ws_client_example.cpp)
target_link_libraries(example_ws_client PRIVATE my_http_lib)

target_compile_definitions(example_ws_client PRIVATE
        DEFAULT_CONFIG_WS_PATH="${CMAKE_SOURCE_DIR}/examples/configs/ws_basic.conf"
)

set_target_properties(example_http_client example_ws_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)