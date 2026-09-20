# zenoh-topology-viewer Recipe

This Recipe connects the C++ collector to the browser through Hakoniwa PDU Endpoint / Bridge.

## Runtime path

```text
collector
  -> TCP Endpoint (9101)
  -> hakoniwa-pdu-bridge
  -> WebSocket Endpoint (8765)
  -> hakoniwa-pdu-javascript
  -> browser
```

The topology payload is:

- PDU: `ZenohTopology/topology`
- type: `std_msgs/String`
- encoding: CDR
- string body: `hakoniwa.zenoh.topology/v1` JSON

The configured `pdu_size` is 256 KiB and is used as bridge receive capacity. The actual CDR payload is variable length.

This Recipe depends on the Bridge behavior that forwards only `received_size` bytes for variable-length PDU payloads.

## Launcher-managed dummy-data demo

The Hakoniwa Launcher from `hakoniwa-pdu-python` owns the Bridge, Vite web
server, and long-running file-backed Collector as one lifecycle.

Start the three processes without invoking `hako-cmd start`:

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

## Manual startup

### 1. Start the Bridge

Assuming `hakoniwa-pdu-bridge` is installed:

```bash
hakoniwa-pdu-bridge \
  recipes/zenoh-topology-viewer/config/bridge/bridge.json \
  1000 \
  recipes/zenoh-topology-viewer/config/endpoint/endpoint_container.json \
  zenoh_topology_bridge
```

### 2. Start the Browser and connect WebSocket

```bash
cd web
npm install
npm run dev
```

Open the Vite URL, keep `ws://127.0.0.1:8765`, and press **Connect**. The
periodic collector below may be started before or after the browser connects.

### 3. Run the Collector with dummy data

```bash
hako-zenoh-topology-collector \
  --input-file web/public/sample-topology.json \
  --endpoint-config recipes/zenoh-topology-viewer/config/endpoint/collector-out.json \
  --publish-interval-ms 1000 \
  --no-stdout
```

This exercises Collector, Endpoint, Bridge, WebSocket, browser-side CDR decode,
and graph rendering without opening a Zenoh session. The collector re-reads and
publishes the file every second until it is stopped. Live Zenoh collection is a
follow-up step.
