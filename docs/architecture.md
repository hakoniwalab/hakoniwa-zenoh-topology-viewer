# Architecture

## Overview

The live topology path is built around **per-session observation**.

Zenoh's Connectivity API exposes transports and links for the **current Zenoh
session**. It does not provide an omniscient graph of every connection in the
deployment. This project reconstructs a broader topology by attaching a
lightweight agent to multiple real application sessions and aggregating their
local observations.

```text
Application A                         Application B
pub/sub/etc.                          pub/sub/etc.
    |                                    |
    | existing Zenoh session             | existing Zenoh session
    v                                    v
Topology Agent A                     Topology Agent B
Connectivity API                     Connectivity API
    |                                    |
    +-------------- TCP -----------------+
                    |
                    v
              TCP multiplexer
                    |
                    v
               Aggregator
          merge / dedupe / stale
                    |
                    v
          aggregated topology v2
                    |
                    v
          Hakoniwa PDU Bridge
                    |
                 WebSocket
                    |
                    v
              Browser Viewer
```

## Topology Agent

The Topology Agent is a small C-compatible observer library.

Applications attach it after opening their Zenoh session:

```text
z_open(...)
   |
   v
hako_topology_agent_attach(z_loan(session), display_name)
```

The agent:

- borrows the application's session and does not take ownership of it;
- does not open a separate Zenoh session;
- periodically queries the Connectivity API;
- reports the local session ZID, transports, and links;
- publishes the snapshot as `std_msgs/String` / CDR through Hakoniwa PDU Endpoint.

Configuration is provided through `HAKO_TOPOLOGY_ENDPOINT_CONFIG`. The optional
`HAKO_TOPOLOGY_INTERVAL_MS` controls the reporting interval.

The agent must be detached before the owning application drops the Zenoh
session.

## Local observation semantics

For a topology such as:

```text
A ----- B ----- C
```

the sessions typically observe:

```text
A: A-B
B: B-A, B-C
C: C-B
```

Therefore one agent cannot reconstruct the full graph. The Aggregator needs
observations from enough sessions to cover the links that should appear in the
viewer.

This is the reason the Topology Agent is attached to the actual publisher /
subscriber sessions instead of running one independent "monitor" session.

## Agent transport

Agents send topology snapshots through Hakoniwa PDU Endpoint.

The current Foundation configuration uses one TCP multiplexer input on the
Aggregator side:

```text
Agent A --\
Agent B ----> TCP multiplexer --> Aggregator
Agent C --/
```

Dynamic targets are supported, so the Aggregator can accept agents without
assigning a dedicated listening port to each one.

## Aggregator

The Aggregator receives per-session snapshots and maintains the most recent
observation for each source.

Its responsibilities include:

- identifying sources by ZID while preserving human-readable display names;
- merging nodes from multiple observations;
- retaining transport observations;
- canonicalizing and deduplicating links;
- recording link provenance in `observed_by`;
- detecting missing updates and marking sources `stale`;
- producing overall `complete` or `partial` status.

The aggregate output schema is:

```text
hakoniwa.zenoh.topology/v2
```

Source status values include:

```text
ok / waiting / error / stale
```

Stale snapshots remain available for visualization so the browser can show
which part of the topology stopped reporting instead of immediately erasing the
previous graph.

## Browser path

The Aggregator publishes the merged JSON through the Hakoniwa PDU path:

```text
Aggregator
  -> std_msgs/String / CDR
  -> hakoniwa-pdu-endpoint
  -> TCP
  -> hakoniwa-pdu-bridge-core
  -> WebSocket / CDR
  -> hakoniwa-pdu-javascript
  -> JSON.parse()
  -> Cytoscape.js
```

The browser renders nodes and links, preserves layout/selection across periodic
snapshot updates, and visually distinguishes stale sources.

See [viewer-guide.md](viewer-guide.md) for the display semantics.

## Direct collector mode

The collector executable can still open its own Zenoh session and emit one
Connectivity API snapshot. This is useful for local inspection and testing, but
it only describes the direct connectivity visible from that collector session.

```text
hako-zenoh-topology-collector
  -> one local Zenoh session
  -> one local connectivity snapshot
```

This mode is different from the multi-agent Aggregator architecture used by the
live Viewer exercise.

## Dummy input mode

For pipeline regression testing the collector can read a JSON file instead of
opening Zenoh:

```text
sample-topology.json
  -> collector
  -> Endpoint
  -> Bridge
  -> WebSocket
  -> Browser
```

This isolates the Hakoniwa PDU / browser path and is intentionally retained as a
development smoke test.

## Connectivity API dependency

The collector and Topology Agent use Zenoh's Connectivity API. In zenoh-c this
currently requires an unstable-API build:

```text
ZENOHC_BUILD_WITH_UNSTABLE_API=true
```

## Workspace boundary

Repository source and Business Pack managed workspace are separate.

Hakoniwa Business Pack owns generated runtime/build state under:

```text
work/foundation/
work/recipes/
```

This repository provides:

- component source;
- JSON schemas;
- Foundation runtime configuration templates;
- component-owned `tools/hako.py`;
- browser assets and documentation.

For the complete lecture-oriented setup, use the
[zenoh-tutorial Viewer exercise](https://github.com/tmori/zenoh-tutorial/blob/main/docs/06-viewer-setup.md).
