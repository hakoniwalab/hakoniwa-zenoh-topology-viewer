#pragma once

#include <memory>
#include <string>

namespace hakoniwa::pdu {
class Endpoint;
}

namespace hako::zenoh_topology {

class TopologyPduPublisher {
public:
    explicit TopologyPduPublisher(std::string endpoint_config_path);
    ~TopologyPduPublisher();

    TopologyPduPublisher(const TopologyPduPublisher&) = delete;
    TopologyPduPublisher& operator=(const TopologyPduPublisher&) = delete;

    void start();
    void publish_json(const std::string& json);
    void stop() noexcept;

private:
    std::string endpoint_config_path_;
    std::unique_ptr<hakoniwa::pdu::Endpoint> endpoint_;
    bool started_{false};
};

}  // namespace hako::zenoh_topology
