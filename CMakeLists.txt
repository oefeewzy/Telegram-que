cmake_minimum_required(VERSION 3.15)
project(delayed_message)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(delayed_message main.cpp)

target_link_libraries(delayed_message
    tdclient
    tdcore
    tddb
    tdjson_static
    Threads::Threads
)