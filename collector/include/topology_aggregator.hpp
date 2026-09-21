#pragma once

#include <string>
#include <vector>

#include "topology_model.hpp"

namespace hako::zenoh_topology {

struct InventoryTarget {
    std::string name;
    std::string role;
    std::string endpoint_config;
    std::string expected_zid;
};

struct Inventory {
    std::uint64_t refresh_interval_ms{1000};
    std::uint64_t stale_after_ms{5000};
    std::string endpoint_mux_config;
    bool dynamic_targets{false};
    std::vector<InventoryTarget> targets;
};

struct TargetObservation {
    InventoryTarget target;
    TopologySnapshot snapshot;
    std::string status{"ok"};
    std::string error;
    std::optional<std::uint64_t> last_received_at_ms;
};

Inventory load_inventory(const std::string& path);
TopologySnapshot aggregate_topology(
    const std::vector<TargetObservation>& observations,
    const std::vector<ObservationSource>& failed_sources = {});

}  // namespace hako::zenoh_topology
