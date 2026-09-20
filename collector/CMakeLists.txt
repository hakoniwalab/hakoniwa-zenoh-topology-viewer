cmake_minimum_required(VERSION 3.16)
project(hakoniwa_zenoh_topology_collector LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HAKO_ZENOH_TOPOLOGY_BUILD_TESTS "Build collector unit tests" ON)
option(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH "Enable live Zenoh connectivity collection" ON)
set(HAKO_PDU_REGISTRY_ROOT "" CACHE PATH "Path to hakoniwa-pdu-registry source tree")

find_package(hakoniwa_pdu_endpoint CONFIG REQUIRED)
find_package(fastcdr CONFIG REQUIRED)

if(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
    find_package(zenohc CONFIG REQUIRED)
    if(NOT ZENOHC_BUILD_WITH_UNSTABLE_API)
        message(FATAL_ERROR
            "zenoh-c must be built with ZENOHC_BUILD_WITH_UNSTABLE_API=true "
            "because the Connectivity API is currently unstable.")
    endif()
endif()

if(NOT HAKO_PDU_REGISTRY_ROOT)
    message(FATAL_ERROR
        "HAKO_PDU_REGISTRY_ROOT is required to use the generated std_msgs/String CDR converter.")
endif()

set(HAKO_PDU_TYPES_DIR "${HAKO_PDU_REGISTRY_ROOT}/pdu/types")
if(NOT EXISTS "${HAKO_PDU_TYPES_DIR}/std_msgs/pdu_cpptype_cdr_conv_String.hpp")
    message(FATAL_ERROR
        "std_msgs/String CDR converter was not found under HAKO_PDU_REGISTRY_ROOT=${HAKO_PDU_REGISTRY_ROOT}")
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
    src/topology_pdu_publisher.cpp
)

if(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
    target_sources(hako-zenoh-topology-collector PRIVATE src/zenoh_collector.cpp)
endif()

target_include_directories(hako-zenoh-topology-collector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${HAKO_PDU_TYPES_DIR}
)

if(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
    target_compile_definitions(hako-zenoh-topology-collector
        PRIVATE
            HAKO_ZENOH_TOPOLOGY_WITH_ZENOH
            Z_FEATURE_UNSTABLE_API
    )
    if(ZENOHC_BUILD_WITH_SHARED_MEMORY)
        target_compile_definitions(hako-zenoh-topology-collector
            PRIVATE
                Z_FEATURE_SHARED_MEMORY
        )
    endif()
endif()

target_link_libraries(hako-zenoh-topology-collector
    PRIVATE
        hako_zenoh_topology_model
        hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint
        fastcdr
)

if(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
    target_link_libraries(hako-zenoh-topology-collector PRIVATE zenohc::shared)
endif()

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
