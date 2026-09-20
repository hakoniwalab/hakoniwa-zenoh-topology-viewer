#include "topology_pdu_subscriber.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "hakoniwa/pdu/endpoint.hpp"
#include "std_msgs/pdu_cpptype_cdr_conv_String.hpp"

namespace hako::zenoh_topology {

TopologyPduSubscriber::TopologyPduSubscriber(std::string endpoint_config_path)
    : endpoint_config_path_(std::move(endpoint_config_path)),
      endpoint_(std::make_unique<hakoniwa::pdu::Endpoint>(
          "zenoh_topology_aggregator_input", HAKO_PDU_ENDPOINT_DIRECTION_IN))
{
}

TopologyPduSubscriber::~TopologyPduSubscriber()
{
    stop();
}

void TopologyPduSubscriber::start()
{
    if (started_) return;
    if (endpoint_->open(endpoint_config_path_) != HAKO_PDU_ERR_OK) {
        throw std::runtime_error("failed to open aggregator input endpoint: " + endpoint_config_path_);
    }
    if (endpoint_->start() != HAKO_PDU_ERR_OK) {
        (void)endpoint_->close();
        throw std::runtime_error("failed to start aggregator input endpoint: " + endpoint_config_path_);
    }
    bool running = false;
    for (int i = 0; i < 50; ++i) {
        if (endpoint_->is_running(running) == HAKO_PDU_ERR_OK && running) {
            started_ = true;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stop();
    throw std::runtime_error("aggregator input endpoint did not become ready: " + endpoint_config_path_);
}

std::optional<std::string> TopologyPduSubscriber::receive_json()
{
    if (!started_) throw std::runtime_error("aggregator input endpoint is not started");
    std::vector<std::byte> buffer(256U * 1024U);
    std::size_t received_size = 0;
    const hakoniwa::pdu::PduKey key{"ZenohTopology", "topology"};
    const auto err = endpoint_->recv(key, std::span<std::byte>(buffer.data(), buffer.size()), received_size);
    if (err == HAKO_PDU_ERR_NO_ENTRY || received_size == 0) return std::nullopt;
    if (err != HAKO_PDU_ERR_OK) {
        throw std::runtime_error("failed to receive topology PDU: error=" + std::to_string(static_cast<int>(err)));
    }

    std::vector<std::uint8_t> payload(received_size);
    for (std::size_t i = 0; i < received_size; ++i) payload[i] = std::to_integer<std::uint8_t>(buffer[i]);
    HakoCpp_String value{};
    hako::pdu::msgs::std_msgs::StringCdr converter;
    if (!converter.cdr2cpp(payload, value)) throw std::runtime_error("failed to decode topology PDU");
    return value.data;
}

void TopologyPduSubscriber::stop() noexcept
{
    if (!endpoint_) return;
    if (started_) {
        (void)endpoint_->stop();
        started_ = false;
    }
    (void)endpoint_->close();
}

}  // namespace hako::zenoh_topology
