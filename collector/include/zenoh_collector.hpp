#pragma once

#include <optional>
#include <memory>
#include <string>

#include "topology_model.hpp"
#include "zenoh.h"

namespace hako::zenoh_topology {

class ZenohCollector {
public:
    explicit ZenohCollector(std::optional<std::string> config_path = std::nullopt);
    ~ZenohCollector();

    ZenohCollector(const ZenohCollector&) = delete;
    ZenohCollector& operator=(const ZenohCollector&) = delete;

    TopologySnapshot collect_once() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

TopologySnapshot collect_session_topology(const z_loaned_session_t* session);

}  // namespace hako::zenoh_topology
