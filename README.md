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

## Runtime architecture

```text
Zenoh
  -> C++ Topology Collector
  -> std_msgs/String / CDR (JSON payload)
  -> hakoniwa-pdu-endpoint
  -> TCP
  -> hakoniwa-pdu-bridge-core
  -> WebSocket / CDR
  -> hakoniwa-pdu-javascript
  -> JSON.parse()
  -> Cytoscape.js
```

The bridge-side `pdu_size` is a receive-capacity bound. The actual CDR string payload remains variable length.

## Collector build dependencies

- zenoh-c built with `ZENOHC_BUILD_WITH_UNSTABLE_API=true`
- hakoniwa-pdu-endpoint
- Fast-CDR
- hakoniwa-pdu-registry source tree for the generated `std_msgs/String` CDR converter

Example:

```bash
cmake -S collector -B collector/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/zenoh-c-install;/path/to/hakoniwa-foundation-install" \
  -DHAKO_PDU_REGISTRY_ROOT=/path/to/hakoniwa-pdu-registry

cmake --build collector/build
ctest --test-dir collector/build --output-on-failure
```

## Collector output

Stdout only:

```bash
./collector/build/hako-zenoh-topology-collector
```

Publish the same JSON through Hakoniwa PDU Endpoint:

```bash
./collector/build/hako-zenoh-topology-collector \
  --endpoint-config recipes/zenoh-topology-viewer/config/endpoint/collector-out.json
```

See `recipes/zenoh-topology-viewer/README.md` for the end-to-end path.
