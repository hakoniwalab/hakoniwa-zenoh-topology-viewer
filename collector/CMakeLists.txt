cmake_minimum_required(VERSION 3.16)
project(hakoniwa_zenoh_topology_collector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hako-zenoh-topology-collector
    src/main.cpp
)

target_include_directories(hako-zenoh-topology-collector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# TODO:
# - link zenoh-cpp / zenoh-c
# - link hakoniwa-pdu-endpoint
# - add topology collector implementation
