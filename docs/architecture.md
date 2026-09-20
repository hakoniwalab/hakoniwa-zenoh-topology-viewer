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

Repository source and Business Pack managed workspace are separate.

The Business Pack owns generated runtime state under:

```text
work/foundation/
work/recipes/zenoh-topology-viewer/
```

This repository provides component source, schemas, Recipe source definitions, and component-owned operational tooling.
