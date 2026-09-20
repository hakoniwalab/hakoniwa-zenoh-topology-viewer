#include "hako_zenoh_topology_agent.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "topology_json.hpp"
#include "topology_pdu_publisher.hpp"
#include "zenoh_collector.hpp"

namespace {

class AgentRuntime {
public:
    AgentRuntime(const z_loaned_session_t* session, std::string node_name, std::string endpoint, std::uint64_t interval_ms)
        : session_(session), node_name_(std::move(node_name)), publisher_(std::move(endpoint)), interval_ms_(interval_ms)
    {
        publisher_.start();
        worker_ = std::thread([this] { run(); });
    }

    ~AgentRuntime()
    {
        stop_.store(true);
        if (worker_.joinable()) worker_.join();
    }

private:
    void run() noexcept
    {
        while (!stop_.load()) {
            try {
                auto snapshot = hako::zenoh_topology::collect_session_topology(session_);
                publisher_.publish_json(hako::zenoh_topology::to_json(snapshot));
            } catch (const std::exception& e) {
                std::cerr << "topology agent [" << node_name_ << "] error: " << e.what() << std::endl;
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms_);
            while (!stop_.load() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    const z_loaned_session_t* session_;
    std::string node_name_;
    hako::zenoh_topology::TopologyPduPublisher publisher_;
    std::uint64_t interval_ms_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

std::mutex agent_mutex;
std::unique_ptr<AgentRuntime> agent;

}  // namespace

extern "C" int hako_topology_agent_attach(const z_loaned_session_t* session, const char* node_name)
{
    if (session == nullptr) return -1;
    const char* endpoint = std::getenv("HAKO_TOPOLOGY_ENDPOINT_CONFIG");
    if (endpoint == nullptr || endpoint[0] == '\0') {
        std::cerr << "HAKO_TOPOLOGY_ENDPOINT_CONFIG is required when the topology agent is enabled" << std::endl;
        return -1;
    }
    std::uint64_t interval_ms = 1000;
    if (const char* value = std::getenv("HAKO_TOPOLOGY_INTERVAL_MS"); value != nullptr) {
        try {
            interval_ms = std::stoull(value);
            if (interval_ms == 0) return -1;
        } catch (const std::exception&) {
            return -1;
        }
    }
    try {
        std::lock_guard lock(agent_mutex);
        if (agent) return -1;
        agent = std::make_unique<AgentRuntime>(session, node_name == nullptr ? "" : node_name, endpoint, interval_ms);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "failed to attach topology agent: " << e.what() << std::endl;
        return -1;
    }
}

extern "C" void hako_topology_agent_detach(void)
{
    std::unique_ptr<AgentRuntime> stopping;
    {
        std::lock_guard lock(agent_mutex);
        stopping = std::move(agent);
    }
    stopping.reset();
}
