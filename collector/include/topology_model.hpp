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
    std::string remote_zid;
    std::string remote_mode{"unknown"};
    bool qos{false};
    bool multicast{false};
    std::optional<bool> shm;
};

struct LinkInfo {
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
};

struct TopologySnapshot {
    std::string schema{"hakoniwa.zenoh.topology/v1"};
    std::uint64_t timestamp_ms{0};
    std::string collector_zid;
    std::vector<NodeInfo> nodes;
    std::vector<TransportInfo> transports;
    std::vector<LinkInfo> links;
};

}  // namespace hako::zenoh_topology
