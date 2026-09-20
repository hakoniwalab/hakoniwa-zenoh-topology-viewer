# hakoniwa-zenoh-topology-viewer

Hakoniwa-based Zenoh topology collector and browser viewer.

## Project layout

```text
collector/   C++ Zenoh topology collector
web/         Browser graph viewer
schema/      Topology JSON schema
recipes/     Recipe-local configuration templates
tools/       Component-owned hako.py
docs/        Architecture notes
prebuilt/    Prebuilt distribution metadata/artifacts
```

## Initial collector

The first collector implementation prints one local connectivity snapshot as JSON.

It uses the Zenoh Connectivity API, which is currently exposed by `zenoh-c` as an unstable API. Build zenoh-c with:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DZENOHC_BUILD_WITH_UNSTABLE_API=true \
  -DCMAKE_INSTALL_PREFIX=/path/to/zenoh-c-install
cmake --build build --config Release
cmake --install build --config Release
```

Then build the collector:

```bash
cmake -S collector -B collector/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/zenoh-c-install
cmake --build collector/build
ctest --test-dir collector/build --output-on-failure
```

Run:

```bash
./collector/build/hako-zenoh-topology-collector
```

Or use a Zenoh JSON5 config:

```bash
./collector/build/hako-zenoh-topology-collector --config ./zenoh.json5
```

The MVP transports a topology snapshot as JSON inside `std_msgs/String`, encoded with CDR and delivered through Hakoniwa PDU Endpoint / Bridge to the browser. The PDU output is a follow-up step; this collector PR validates Zenoh connectivity discovery and JSON serialization first.
