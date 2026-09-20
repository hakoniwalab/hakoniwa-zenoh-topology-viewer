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
    std::vector<InventoryTarget> targets;
};

struct TargetObservation {
    InventoryTarget target;
    TopologySnapshot snapshot;
};

Inventory load_inventory(const std::string& path);
TopologySnapshot aggregate_topology(
    const std::vector<TargetObservation>& observations,
    const std::vector<ObservationSource>& failed_sources = {});

}  // namespace hako::zenoh_topology
