#pragma once

#include <string>

#include "topology_model.hpp"

namespace hako::zenoh_topology {

std::string to_json(const TopologySnapshot& snapshot);
TopologySnapshot topology_from_json(const std::string& json);

}  // namespace hako::zenoh_topology
