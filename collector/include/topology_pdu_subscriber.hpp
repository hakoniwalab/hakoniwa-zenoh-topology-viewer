#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace hakoniwa::pdu {
class Endpoint;
}

namespace hako::zenoh_topology {

class TopologyPduSubscriber {
public:
    explicit TopologyPduSubscriber(std::string endpoint_config_path);
    ~TopologyPduSubscriber();

    TopologyPduSubscriber(const TopologyPduSubscriber&) = delete;
    TopologyPduSubscriber& operator=(const TopologyPduSubscriber&) = delete;

    void start();
    std::optional<std::string> receive_json();
    void stop() noexcept;

private:
    std::string endpoint_config_path_;
    std::unique_ptr<hakoniwa::pdu::Endpoint> endpoint_;
    std::size_t pdu_size_{0};
    bool started_{false};
};

}  // namespace hako::zenoh_topology
