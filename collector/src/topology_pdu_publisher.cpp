#include "topology_pdu_publisher.hpp"

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
namespace {

std::vector<std::byte> to_bytes(const std::vector<std::uint8_t>& src)
{
    std::vector<std::byte> out(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        out[i] = static_cast<std::byte>(src[i]);
    }
    return out;
}

}  // namespace

TopologyPduPublisher::TopologyPduPublisher(std::string endpoint_config_path)
    : endpoint_config_path_(std::move(endpoint_config_path)),
      endpoint_(std::make_unique<hakoniwa::pdu::Endpoint>(
          "zenoh_topology_collector",
          HAKO_PDU_ENDPOINT_DIRECTION_OUT))
{
}

TopologyPduPublisher::~TopologyPduPublisher()
{
    stop();
}

void TopologyPduPublisher::start()
{
    if (started_) {
        return;
    }

    if (endpoint_->open(endpoint_config_path_) != HAKO_PDU_ERR_OK) {
        throw std::runtime_error("failed to open Hakoniwa endpoint: " + endpoint_config_path_);
    }
    if (endpoint_->start() != HAKO_PDU_ERR_OK) {
        (void)endpoint_->close();
        throw std::runtime_error("failed to start Hakoniwa endpoint");
    }

    bool running = false;
    for (int i = 0; i < 50; ++i) {
        if (endpoint_->is_running(running) == HAKO_PDU_ERR_OK && running) {
            started_ = true;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    (void)endpoint_->stop();
    (void)endpoint_->close();
    throw std::runtime_error("Hakoniwa endpoint did not become ready");
}

void TopologyPduPublisher::publish_json(const std::string& json)
{
    if (!started_) {
        throw std::runtime_error("Hakoniwa endpoint is not started");
    }

    HakoCpp_String value{};
    value.data = json;

    hako::pdu::msgs::std_msgs::StringCdr converter;
    std::vector<std::uint8_t> cdr_payload;
    if (converter.cpp2cdr(value, cdr_payload) < 0) {
        throw std::runtime_error("failed to encode topology JSON as std_msgs/String CDR");
    }

    const auto payload = to_bytes(cdr_payload);
    const hakoniwa::pdu::PduKey key{"ZenohTopology", "topology"};
    const auto err = endpoint_->send(
        key,
        std::span<const std::byte>(payload.data(), payload.size()));

    if (err != HAKO_PDU_ERR_OK) {
        throw std::runtime_error(
            "failed to send topology PDU: error=" + std::to_string(static_cast<int>(err)));
    }
}

void TopologyPduPublisher::stop() noexcept
{
    if (!endpoint_) {
        return;
    }
    if (started_) {
        (void)endpoint_->stop();
        started_ = false;
    }
    (void)endpoint_->close();
}

}  // namespace hako::zenoh_topology
