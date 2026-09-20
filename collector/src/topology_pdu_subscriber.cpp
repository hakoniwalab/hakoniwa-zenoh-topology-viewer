#include "topology_pdu_subscriber.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_comm_multiplexer.hpp"
#include "std_msgs/pdu_cpptype_cdr_conv_String.hpp"

namespace hako::zenoh_topology {

TopologyPduSubscriber::TopologyPduSubscriber(std::string endpoint_config_path)
    : endpoint_config_path_(std::move(endpoint_config_path)),
      endpoint_(std::make_unique<hakoniwa::pdu::Endpoint>(
          "zenoh_topology_aggregator_input", HAKO_PDU_ENDPOINT_DIRECTION_IN))
{
}

TopologyPduSubscriber::TopologyPduSubscriber(
    std::unique_ptr<hakoniwa::pdu::Endpoint> endpoint,
    std::string endpoint_config_path)
    : endpoint_config_path_(std::move(endpoint_config_path)),
      endpoint_(std::move(endpoint)),
      started_(true)
{
    if (!endpoint_) throw std::runtime_error("multiplexer returned an empty endpoint");
    const hakoniwa::pdu::PduKey key{"ZenohTopology", "topology"};
    pdu_size_ = endpoint_->get_pdu_size(key);
    if (pdu_size_ == 0) {
        stop();
        throw std::runtime_error("topology PDU definition was not found: " + endpoint_config_path_);
    }
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
    const hakoniwa::pdu::PduKey key{"ZenohTopology", "topology"};
    pdu_size_ = endpoint_->get_pdu_size(key);
    if (pdu_size_ == 0) {
        (void)endpoint_->close();
        throw std::runtime_error("topology PDU definition was not found: " + endpoint_config_path_);
    }
    if (endpoint_->start() != HAKO_PDU_ERR_OK) {
        (void)endpoint_->close();
        throw std::runtime_error("failed to start aggregator input endpoint: " + endpoint_config_path_);
    }
    // This input endpoint is a TCP server. Listening successfully is enough to
    // make the aggregator ready; is_running() also requires a connected client.
    started_ = true;
}

std::optional<std::string> TopologyPduSubscriber::receive_json()
{
    if (!started_) throw std::runtime_error("aggregator input endpoint is not started");
    std::vector<std::byte> buffer(pdu_size_);
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

bool TopologyPduSubscriber::is_running() noexcept
{
    if (!started_) return false;
    bool running = false;
    return endpoint_->is_running(running) == HAKO_PDU_ERR_OK && running;
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

TopologyPduMultiplexer::TopologyPduMultiplexer(std::string endpoint_config_path)
    : endpoint_config_path_(std::move(endpoint_config_path)),
      multiplexer_(std::make_unique<hakoniwa::pdu::EndpointCommMultiplexer>(
          "zenoh_topology_aggregator_mux", HAKO_PDU_ENDPOINT_DIRECTION_IN))
{
}

TopologyPduMultiplexer::~TopologyPduMultiplexer()
{
    stop();
}

void TopologyPduMultiplexer::start()
{
    if (started_) return;
    if (multiplexer_->open(endpoint_config_path_) != HAKO_PDU_ERR_OK) {
        throw std::runtime_error("failed to open aggregator input multiplexer: " + endpoint_config_path_);
    }
    if (multiplexer_->start() != HAKO_PDU_ERR_OK) {
        (void)multiplexer_->close();
        throw std::runtime_error("failed to start aggregator input multiplexer: " + endpoint_config_path_);
    }
    started_ = true;
}

std::vector<std::unique_ptr<TopologyPduSubscriber>> TopologyPduMultiplexer::take_subscribers()
{
    if (!started_) throw std::runtime_error("aggregator input multiplexer is not started");
    std::vector<std::unique_ptr<TopologyPduSubscriber>> subscribers;
    for (auto& endpoint : multiplexer_->take_endpoints()) {
        subscribers.push_back(std::make_unique<TopologyPduSubscriber>(
            std::move(endpoint), endpoint_config_path_));
    }
    return subscribers;
}

void TopologyPduMultiplexer::stop() noexcept
{
    if (!multiplexer_) return;
    if (started_) {
        (void)multiplexer_->stop();
        started_ = false;
    }
    (void)multiplexer_->close();
}

}  // namespace hako::zenoh_topology
