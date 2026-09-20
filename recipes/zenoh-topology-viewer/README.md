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

## Bridge

Assuming `hakoniwa-pdu-bridge` is installed:

```bash
hakoniwa-pdu-bridge \
  recipes/zenoh-topology-viewer/config/bridge/bridge.json \
  1000 \
  recipes/zenoh-topology-viewer/config/endpoint/endpoint_container.json \
  zenoh_topology_bridge
```

## Collector

```bash
hako-zenoh-topology-collector \
  --endpoint-config recipes/zenoh-topology-viewer/config/endpoint/collector-out.json
```

## Browser

```bash
cd web
npm install
npm run dev
```

Open the Vite URL, keep `ws://127.0.0.1:8765`, and press **Connect**.
