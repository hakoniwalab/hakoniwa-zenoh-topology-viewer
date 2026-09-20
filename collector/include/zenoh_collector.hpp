#pragma once

#include <optional>
#include <string>

#include "topology_model.hpp"

namespace hako::zenoh_topology {

class ZenohCollector {
public:
    explicit ZenohCollector(std::optional<std::string> config_path = std::nullopt);

    TopologySnapshot collect_once() const;

private:
    std::optional<std::string> config_path_;
};

}  // namespace hako::zenoh_topology
