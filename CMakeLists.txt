cmake_minimum_required(VERSION 3.2)

project(Nano_httpd)

add_executable(${PROJECT_NAME} 
    src/http_conn.c
    src/threadpool.c
    src/http_parser.c
    src/list.c
    src/Nano_server.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(${PROJECT_NAME}
    PRIVATE 
        -g 
        -Wall
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        pthread
)
