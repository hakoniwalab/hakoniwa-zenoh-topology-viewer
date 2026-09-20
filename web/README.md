# Web viewer

Browser-side renderer for Zenoh topology snapshots.

## Current scope

This stage renders a dummy `hakoniwa.zenoh.topology/v1` JSON snapshot with Cytoscape.js.

The dummy JSON does not bypass the runtime pipeline. The collector reads the
file and publishes it through Hakoniwa PDU Endpoint, Bridge, and WebSocket before
the browser renders it. Zenoh itself is not required for this stage.

## Run

```bash
cd web
npm install
npm run dev
```

Open the URL shown by Vite.

Connect to `ws://127.0.0.1:8765` before starting the one-shot collector.

## Test

```bash
npm test
```

## Build

```bash
npm run build
```

The production bundle is written to `web/dist/`.

## Next integration

The current dummy-data path is:

```text
sample-topology.json
  -> collector --input-file
  -> Hakoniwa PDU Endpoint
  -> Bridge / WebSocket
  -> hakoniwa-pdu-javascript
  -> std_msgs/String CDR decode
  -> JSON.parse(pdu.data)
  -> TopologyView.render(snapshot)
```
