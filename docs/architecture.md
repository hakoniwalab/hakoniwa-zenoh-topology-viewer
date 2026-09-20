# Architecture

Initial MVP flow:

```text
Zenoh Network
    |
    v
C++ Topology Collector
    |
    | std_msgs/String / CDR
    | payload: topology JSON
    v
hakoniwa-pdu-endpoint
    |
    v
hakoniwa-pdu-bridge-core
    |
    | WebSocket / CDR
    v
hakoniwa-pdu-javascript
    |
    | CDR decode -> JSON.parse()
    v
Browser Graph Viewer
```

## Collector scope

The first collector uses Zenoh's Connectivity API and emits a single JSON snapshot to stdout.

The Connectivity API exposes the transports and links of the collector's own Zenoh session. Therefore this first snapshot represents **direct connectivity visible from the collector node**, not an omniscient graph of every connection in the deployment.

Whole-network aggregation is a later step. Candidate approaches include querying Admin Space for multiple nodes or deploying/aggregating per-node observations.

The Connectivity API is currently an unstable zenoh-c API, so zenoh-c must be built with `ZENOHC_BUILD_WITH_UNSTABLE_API=true`.

## Workspace boundary

Repository source and Business Pack managed workspace are separate.

The Business Pack owns generated runtime state under:

```text
work/foundation/
work/recipes/zenoh-topology-viewer/
```

This repository provides component source, schemas, Recipe source definitions, and component-owned operational tooling.
