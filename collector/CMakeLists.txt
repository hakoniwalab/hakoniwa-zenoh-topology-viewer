cmake_minimum_required(VERSION 3.16)
project(hakoniwa_zenoh_topology_collector LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HAKO_ZENOH_TOPOLOGY_BUILD_TESTS "Build collector unit tests" ON)

find_package(zenohc CONFIG REQUIRED)

if(NOT ZENOHC_BUILD_WITH_UNSTABLE_API)
    message(FATAL_ERROR
        "zenoh-c must be built with ZENOHC_BUILD_WITH_UNSTABLE_API=true "
        "because the Connectivity API is currently unstable.")
endif()

add_library(hako_zenoh_topology_model
    src/topology_json.cpp
)

target_include_directories(hako_zenoh_topology_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(hako-zenoh-topology-collector
    src/main.cpp
    src/zenoh_collector.cpp
)

target_include_directories(hako-zenoh-topology-collector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(hako-zenoh-topology-collector
    PRIVATE
        Z_FEATURE_UNSTABLE_API
)

if(ZENOHC_BUILD_WITH_SHARED_MEMORY)
    target_compile_definitions(hako-zenoh-topology-collector
        PRIVATE
            Z_FEATURE_SHARED_MEMORY
    )
endif()

target_link_libraries(hako-zenoh-topology-collector
    PRIVATE
        hako_zenoh_topology_model
        zenohc::shared
)

install(TARGETS hako-zenoh-topology-collector
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(HAKO_ZENOH_TOPOLOGY_BUILD_TESTS)
    enable_testing()

    add_executable(test-topology-json
        tests/test_topology_json.cpp
    )
    target_link_libraries(test-topology-json
        PRIVATE
            hako_zenoh_topology_model
    )

    add_test(NAME topology-json COMMAND test-topology-json)
endif()
