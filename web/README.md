# Web viewer

Browser-side renderer for Zenoh topology snapshots.

## Current scope

This stage renders a static/sample `hakoniwa.zenoh.topology/v1` JSON snapshot with Cytoscape.js.

The PDU/WebSocket integration is intentionally deferred to the next step so that graph rendering can be verified independently.

## Run

```bash
cd web
npm install
npm run dev
```

Open the URL shown by Vite.

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

The next step will replace `sample-topology.json` as the live data source:

```text
hakoniwa-pdu-javascript
  -> WebSocket receive
  -> std_msgs/String CDR decode
  -> JSON.parse(pdu.data)
  -> TopologyView.render(snapshot)
```
