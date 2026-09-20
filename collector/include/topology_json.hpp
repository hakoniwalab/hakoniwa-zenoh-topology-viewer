#pragma once

#include <string>

#include "topology_model.hpp"

namespace hako::zenoh_topology {

std::string to_json(const TopologySnapshot& snapshot);

}  // namespace hako::zenoh_topology
