#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hakoniwa::pdu {
class Endpoint;
class EndpointCommMultiplexer;
}

namespace hako::zenoh_topology {

class TopologyPduSubscriber {
public:
    explicit TopologyPduSubscriber(std::string endpoint_config_path);
    TopologyPduSubscriber(
        std::unique_ptr<hakoniwa::pdu::Endpoint> endpoint,
        std::string endpoint_config_path);
    ~TopologyPduSubscriber();

    TopologyPduSubscriber(const TopologyPduSubscriber&) = delete;
    TopologyPduSubscriber& operator=(const TopologyPduSubscriber&) = delete;

    void start();
    std::optional<std::string> receive_json();
    bool is_running() noexcept;
    void stop() noexcept;

private:
    std::string endpoint_config_path_;
    std::unique_ptr<hakoniwa::pdu::Endpoint> endpoint_;
    std::size_t pdu_size_{0};
    bool started_{false};
};

class TopologyPduMultiplexer {
public:
    explicit TopologyPduMultiplexer(std::string endpoint_config_path);
    ~TopologyPduMultiplexer();

    TopologyPduMultiplexer(const TopologyPduMultiplexer&) = delete;
    TopologyPduMultiplexer& operator=(const TopologyPduMultiplexer&) = delete;

    void start();
    std::vector<std::unique_ptr<TopologyPduSubscriber>> take_subscribers();
    void stop() noexcept;

private:
    std::string endpoint_config_path_;
    std::unique_ptr<hakoniwa::pdu::EndpointCommMultiplexer> multiplexer_;
    bool started_{false};
};

}  // namespace hako::zenoh_topology
