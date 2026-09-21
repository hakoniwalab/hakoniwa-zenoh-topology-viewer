# zenoh-topology-viewer development Recipe

This directory contains the **development / dummy-data** Recipe assets kept in
this repository.

For the current live multi-agent topology exercise, use Hakoniwa Business Pack
and the tutorial setup documented here:

- [zenoh-tutorial: 06 ブラウザでZenoh接続Topologyを確認する](https://github.com/tmori/zenoh-tutorial/blob/main/docs/06-viewer-setup.md)

The live architecture is:

```text
real Zenoh sessions
  -> attached Topology Agents
  -> Hakoniwa PDU Endpoint / TCP multiplexer
  -> Aggregator
  -> aggregated topology v2
  -> Hakoniwa PDU Endpoint
  -> PDU Bridge
  -> WebSocket
  -> Browser
```

See [../../docs/architecture.md](../../docs/architecture.md) for details.

## Purpose of this local Recipe

The files under `recipes/zenoh-topology-viewer/` are still useful for a
small, Zenoh-independent regression path:

```text
web/public/sample-topology.json
  -> C++ collector
  -> Hakoniwa PDU Endpoint
  -> TCP
  -> hakoniwa-pdu-bridge-core
  -> WebSocket
  -> hakoniwa-pdu-javascript
  -> browser
```

This verifies the collector output, PDU transport, Bridge, WebSocket, browser
CDR conversion, and graph rendering without requiring a live Zenoh session.

The dummy input file currently uses the original
`hakoniwa.zenoh.topology/v1` sample schema. Live multi-agent aggregation emits
`hakoniwa.zenoh.topology/v2`.

## PDU transport

The topology payload is carried as:

- PDU: `ZenohTopology/topology`
- type: `std_msgs/String`
- encoding: CDR
- string body: topology JSON

The CDR string payload is variable length. Endpoint/Bridge configuration defines
receive capacity; the actual received payload length is preserved through the
Bridge.

## Launcher-managed dummy demo

The Hakoniwa Launcher can own the Bridge, Vite web server, and long-running
file-backed Collector as one lifecycle.

Start the three processes without invoking Hakoniwa Core:

```bash
PYTHONPATH=../hakoniwa-pdu-python/src \
python -m hakoniwa_pdu.apps.launcher.hako_launcher \
  recipes/zenoh-topology-viewer/launch-dummy.json \
  --background work/recipes/zenoh-topology-viewer/launcher-session.json \
  --mode activate-only
```

Open `http://127.0.0.1:5173`, connect to `ws://127.0.0.1:8765`, and inspect
the topology. The Collector re-reads `web/public/sample-topology.json` every
second, so edits to the dummy file appear without restarting the demo.

Check and stop the complete process set through the Launcher:

```bash
PYTHONPATH=../hakoniwa-pdu-python/src \
python -m hakoniwa_pdu.apps.launcher.hako_launcher_ctl \
  status work/recipes/zenoh-topology-viewer/launcher-session.json

PYTHONPATH=../hakoniwa-pdu-python/src \
python -m hakoniwa_pdu.apps.launcher.hako_launcher_ctl \
  terminate work/recipes/zenoh-topology-viewer/launcher-session.json
```

`HAKO_PDU_BRIDGE_BIN` may override the Bridge executable path.

## Manual dummy startup

### 1. Start the Bridge

Assuming `hakoniwa-pdu-bridge` is installed:

```bash
hakoniwa-pdu-bridge \
  recipes/zenoh-topology-viewer/config/bridge/bridge.json \
  1000 \
  recipes/zenoh-topology-viewer/config/endpoint/endpoint_container.json \
  zenoh_topology_bridge
```

### 2. Start the Browser

```bash
cd web
npm install
npm run dev
```

Open the Vite URL, keep `ws://127.0.0.1:8765`, and press **Connect**.

### 3. Run the file-backed Collector

```bash
hako-zenoh-topology-collector \
  --input-file web/public/sample-topology.json \
  --endpoint-config recipes/zenoh-topology-viewer/config/endpoint/collector-out.json \
  --publish-interval-ms 1000 \
  --no-stdout
```

## Live setup

Do not use this dummy Recipe as the reference architecture for the live Viewer.

The live lecture/demo setup is owned by:

- `tmori/zenoh-tutorial` for the Zenoh exercises and optional agent-enabled samples;
- `hakoniwa-business-pack` for Foundation build/install and Launcher lifecycle;
- this repository for the Topology Agent, Aggregator, schemas, and browser viewer.

See:

- [Architecture](../../docs/architecture.md)
- [Viewer guide](../../docs/viewer-guide.md)
- [Development guide](../../docs/development.md)
