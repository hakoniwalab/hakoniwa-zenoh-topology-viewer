#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hako::zenoh_topology {

struct NodeInfo {
    std::string zid;
    std::string mode{"unknown"};
};

struct TransportInfo {
    std::string source_zid;
    std::string remote_zid;
    std::string remote_mode{"unknown"};
    bool qos{false};
    bool multicast{false};
    std::optional<bool> shm;
};

struct LinkInfo {
    std::string source_zid;
    std::string remote_zid;
    std::string protocol;
    std::string src_endpoint;
    std::string dst_endpoint;
    std::string group;
    std::uint16_t mtu{0};
    bool streamed{false};
    std::vector<std::string> interfaces;
    std::string auth_id;
    std::optional<std::uint8_t> min_priority;
    std::optional<std::uint8_t> max_priority;
    std::optional<std::string> reliability;
    std::vector<std::string> observed_by;
};

struct ObservationSource {
    std::string name;
    std::string role;
    std::string endpoint;
    std::string zid;
    std::string status{"ok"};
    std::string error;
    std::optional<std::uint64_t> last_received_at_ms;
};

struct TopologySnapshot {
    std::string schema{"hakoniwa.zenoh.topology/v1"};
    std::uint64_t timestamp_ms{0};
    std::string collector_zid;
    std::string collector_agent_id;
    std::vector<NodeInfo> nodes;
    std::vector<TransportInfo> transports;
    std::vector<LinkInfo> links;
    std::string status{"complete"};
    std::vector<ObservationSource> sources;
};

}  // namespace hako::zenoh_topology
