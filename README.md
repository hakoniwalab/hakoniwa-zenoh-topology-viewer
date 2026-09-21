# hakoniwa-zenoh-topology-viewer

Hakoniwa-based Zenoh topology observer, aggregator, and browser viewer.

This project visualizes the transport/link topology of running Zenoh sessions.
A lightweight Topology Agent is attached to each application's **existing Zenoh
session**, collects the session-local Connectivity API view, and sends snapshots
to an Aggregator. The Aggregator merges those observations into a deployment
topology and publishes it to the browser through the Hakoniwa PDU pipeline.

ブラウザに表示されるノード、リンク、ラベル、Detailsの読み方は
[Zenoh Topology Viewerの見方](docs/viewer-guide.md)を参照してください。
ソース変更後の再ビルドとFoundationへの反映方法は
[開発・メンテナンスガイド](docs/development.md)を参照してください。

## What it does

- Attaches to the Zenoh session already used by a publisher/subscriber process.
- Uses Zenoh's Connectivity API to observe that session's direct transports and links.
- Sends per-session topology snapshots through Hakoniwa PDU Endpoint over TCP.
- Accepts multiple agents through a TCP multiplexer.
- Aggregates and deduplicates observations by Zenoh ID (ZID).
- Tracks source state such as `ok`, `waiting`, `error`, and `stale`.
- Publishes the aggregated topology as `hakoniwa.zenoh.topology/v2`.
- Bridges the topology PDU to WebSocket and renders it with Cytoscape.js.

A single Connectivity API observation is **not** an omniscient network graph.
It only describes the connectivity visible from that Zenoh session. The full
viewer graph is reconstructed by aggregating observations from the sessions
where the Topology Agent is enabled.

## Runtime architecture

```text
Zenoh application process
  pub / sub / other application logic
              |
              | existing z_owned_session_t
              | hako_topology_agent_attach(...)
              v
       Topology Agent
       Connectivity API
       local transports / links
              |
              | std_msgs/String / CDR
              | topology JSON
              v
     hakoniwa-pdu-endpoint
              |
              | TCP
              v
       TCP multiplexer
              |
              v
          Aggregator
       merge / dedupe
       stale detection
              |
              | hakoniwa.zenoh.topology/v2
              v
     hakoniwa-pdu-endpoint
              |
              | TCP
              v
   hakoniwa-pdu-bridge-core
              |
              | WebSocket / CDR
              v
  hakoniwa-pdu-javascript
              |
              | CDR decode -> JSON.parse()
              v
        Cytoscape.js
        Browser Viewer
```

The Topology Agent borrows the application's Zenoh session and does not create
a separate Zenoh session for observation. It must be detached before the
application drops the owning session.

## Recommended tutorial / live setup

The recommended end-to-end usage is the optional topology-viewer exercise in
`tmori/zenoh-tutorial`:

- [zenoh-tutorial: 06 ブラウザでZenoh接続Topologyを確認する](https://github.com/tmori/zenoh-tutorial/blob/main/docs/06-viewer-setup.md)

That workflow uses Hakoniwa Business Pack to build/install the Foundation
components and Hakoniwa Launcher to start the Aggregator, PDU Bridge, and web
server. The ordinary Zenoh tutorial remains independent of the Viewer; the
Topology Agent is attached only when the sample is run with the optional
`--topology-agent` path.

## Topology data

### Per-session observation

Each agent reports the Connectivity API view of one real Zenoh session:

- local ZID / display name
- directly visible remote ZIDs and modes
- transports
- links
- protocol and endpoint information exposed by Zenoh

### Aggregated topology

The Aggregator combines the latest observations from all connected agents.

The current aggregate schema is:

```text
hakoniwa.zenoh.topology/v2
```

It includes:

- `nodes`
- `transports`
- deduplicated `links`
- `sources` and their current status
- overall `complete` / `partial` status

A link may be observed from both endpoints. `observed_by` records which agents
reported it.

## Foundation component lifecycle

This repository provides a component-owned `tools/hako.py` entry point for
Hakoniwa Business Pack Foundation builds:

```bash
python3 tools/hako.py doctor    --config <build-config>
python3 tools/hako.py configure --config <build-config>
python3 tools/hako.py build     --config <build-config>
python3 tools/hako.py test      --config <build-config>
python3 tools/hako.py install   --config <build-config> --install-dir <foundation-prefix>
python3 tools/hako.py smoke     --config <build-config> --install-dir <foundation-prefix>
```

The component does not clone its dependencies. The Foundation provides the
configured Zenoh C, PDU Endpoint, PDU Registry, and browser JavaScript
dependencies.

The collector/agent side requires zenoh-c built with:

```text
ZENOHC_BUILD_WITH_UNSTABLE_API=true
```

because the Connectivity API is currently unstable.

## Development-only dummy pipeline

A file-backed dummy pipeline is still kept for transport/browser smoke testing:

```text
web/public/sample-topology.json
  -> C++ collector
  -> hakoniwa-pdu-endpoint
  -> TCP
  -> hakoniwa-pdu-bridge-core
  -> WebSocket
  -> hakoniwa-pdu-javascript
  -> Cytoscape.js
```

This path intentionally bypasses Zenoh and is useful for development regression
tests. It is **not** the main live topology architecture.

See [recipes/zenoh-topology-viewer/README.md](recipes/zenoh-topology-viewer/README.md)
for the dummy pipeline.

## Project layout

```text
collector/   Connectivity collector, Topology Agent, Aggregator
config/      Foundation runtime configuration
web/         Browser graph viewer
schema/      Topology / inventory JSON schemas
recipes/     Development/dummy Recipe assets
tools/       Component-owned Foundation lifecycle tooling
docs/        Architecture, viewer, and maintenance guides
prebuilt/    Prebuilt distribution metadata/artifacts
```

## More documentation

- [Architecture](docs/architecture.md)
- [Zenoh Topology Viewerの見方](docs/viewer-guide.md)
- [開発・メンテナンスガイド](docs/development.md)
