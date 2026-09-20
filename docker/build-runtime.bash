#!/usr/bin/env bash
set -euo pipefail

PREFIX=/opt/hakoniwa
BUILD_ROOT=/build
PARALLEL=${CMAKE_BUILD_PARALLEL_LEVEL:-2}

cmake -E remove_directory "${BUILD_ROOT}/src/zenoh-c"
cmake -E make_directory "${BUILD_ROOT}/src/zenoh-c"
tar \
  --exclude='./.git' \
  --exclude='./build' \
  --exclude='./target' \
  -C /workspace/zenoh-c -cf - . \
  | tar -C "${BUILD_ROOT}/src/zenoh-c" -xf -

cmake -S "${BUILD_ROOT}/src/zenoh-c" -B "${BUILD_ROOT}/zenoh-c-rw" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_SHARED_LIBS=ON \
  -DZENOHC_BUILD_WITH_UNSTABLE_API=ON
cmake --build "${BUILD_ROOT}/zenoh-c-rw" --parallel "${PARALLEL}"
cmake --install "${BUILD_ROOT}/zenoh-c-rw"

cmake -S /workspace/hakoniwa-pdu-endpoint -B "${BUILD_ROOT}/endpoint" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_SHARED_LIBS=ON \
  -DHAKO_PDU_ENDPOINT_ENABLE_HAKONIWA_CORE=OFF \
  -DHAKO_PDU_ENDPOINT_BUILD_TOOLS=OFF \
  -DHAKO_PDU_ENDPOINT_BUILD_TESTS=OFF \
  -DHAKO_PDU_ENDPOINT_BUILD_BENCHMARKS=OFF \
  -DHAKO_PDU_ENDPOINT_BUILD_EXAMPLES=OFF
cmake --build "${BUILD_ROOT}/endpoint" --parallel "${PARALLEL}"
cmake --install "${BUILD_ROOT}/endpoint"

cmake -S /workspace/hakoniwa-pdu-bridge-core -B "${BUILD_ROOT}/bridge" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DHAKO_PDU_ENDPOINT_PREFIX="${PREFIX}" \
  -DHAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP=OFF \
  -DHAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE=OFF \
  -DHAKO_PDU_BRIDGE_BUILD_MONITOR=OFF \
  -DHAKO_PDU_BRIDGE_BUILD_TESTS=OFF \
  -DHAKO_PDU_BRIDGE_BUILD_EXAMPLES=OFF
cmake --build "${BUILD_ROOT}/bridge" --parallel "${PARALLEL}"
cmake --install "${BUILD_ROOT}/bridge"

cmake -S /workspace/hakoniwa-zenoh-topology-viewer/collector -B "${BUILD_ROOT}/collector" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_PREFIX_PATH="${PREFIX}" \
  -DHAKO_PDU_REGISTRY_ROOT=/workspace/hakoniwa-pdu-registry \
  -DHAKO_ZENOH_TOPOLOGY_WITH_ZENOH=ON \
  -DHAKO_ZENOH_TOPOLOGY_BUILD_TESTS=ON
cmake --build "${BUILD_ROOT}/collector" --parallel "${PARALLEL}"
ctest --test-dir "${BUILD_ROOT}/collector" --output-on-failure
cmake --install "${BUILD_ROOT}/collector"

cmake -S /workspace/zenoh-tutorial/sample/c-sample -B "${BUILD_ROOT}/c-sample" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DZENOH_C_ROOT="${PREFIX}" \
  -DHAKO_ZENOH_TOPOLOGY_AGENT=ON \
  -DHAKO_ZENOH_TOPOLOGY_AGENT_ROOT="${PREFIX}"
cmake --build "${BUILD_ROOT}/c-sample" --parallel "${PARALLEL}"
cmake -E copy "${BUILD_ROOT}/c-sample/pub" "${PREFIX}/bin/zenoh-tutorial-pub"
cmake -E copy "${BUILD_ROOT}/c-sample/sub" "${PREFIX}/bin/zenoh-tutorial-sub"

touch "${PREFIX}/.topology-runtime-ready"
