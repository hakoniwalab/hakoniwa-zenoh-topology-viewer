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
prebuilt/    Prebuilt distribution metadata/artifacts (future)
```

The MVP transports a topology snapshot as JSON inside `std_msgs/String`, encoded with CDR and delivered through Hakoniwa PDU Endpoint / Bridge to the browser.
