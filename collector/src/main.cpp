#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "topology_json.hpp"
#include "topology_aggregator.hpp"
#include "topology_pdu_publisher.hpp"
#include "topology_pdu_subscriber.hpp"
#if defined(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
#include "zenoh.h"
#include "zenoh_collector.hpp"
#endif

namespace {

struct Options {
    std::optional<std::string> config_path;
    std::optional<std::string> input_file_path;
    std::optional<std::string> endpoint_config_path;
    std::optional<std::string> inventory_path;
    std::uint64_t publish_interval_ms{0};
    bool print_stdout{true};
};

std::string read_text_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open topology input file: " + path);
    }

    std::ostringstream content;
    content << input.rdbuf();
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("failed to read topology input file: " + path);
    }
    if (content.str().empty()) {
        throw std::runtime_error("topology input file is empty: " + path);
    }
    return content.str();
}

std::uint64_t system_now_ms()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

Options parse_args(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--config requires a path");
            }
            options.config_path = argv[++i];
        } else if (arg == "--input-file") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--input-file requires a path");
            }
            options.input_file_path = argv[++i];
        } else if (arg == "--endpoint-config") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--endpoint-config requires a path");
            }
            options.endpoint_config_path = argv[++i];
        } else if (arg == "--inventory") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--inventory requires a path");
            }
            options.inventory_path = argv[++i];
        } else if (arg == "--publish-interval-ms") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--publish-interval-ms requires a value");
            }
            const std::string value = argv[++i];
            std::size_t parsed = 0;
            try {
                options.publish_interval_ms = std::stoull(value, &parsed);
            } catch (const std::exception&) {
                throw std::runtime_error("invalid --publish-interval-ms value: " + value);
            }
            if (parsed != value.size() || options.publish_interval_ms == 0) {
                throw std::runtime_error("--publish-interval-ms must be a positive integer");
            }
        } else if (arg == "--no-stdout") {
            options.print_stdout = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: hako-zenoh-topology-collector [OPTIONS]\n\n"
                << "Collect one Zenoh topology snapshot.\n\n"
                << "Options:\n"
                << "  -c, --config <zenoh.json5>       Zenoh session config\n"
                << "      --input-file <topology.json> Read a topology snapshot instead of Zenoh\n"
                << "      --endpoint-config <json>     Hakoniwa Endpoint config; publishes CDR std_msgs/String\n"
                << "      --inventory <json>           Aggregate topology agents defined in an inventory\n"
                << "      --publish-interval-ms <ms>   Refresh file/inventory and publish periodically\n"
                << "      --no-stdout                  Do not print the JSON snapshot\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    const auto source_count = static_cast<int>(options.input_file_path.has_value())
        + static_cast<int>(options.inventory_path.has_value());
    if (source_count > 1 || (options.config_path.has_value() && source_count > 0)) {
        throw std::runtime_error("--config, --input-file and --inventory select mutually exclusive sources");
    }
    return options;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        const auto options = parse_args(argc, argv);
        std::optional<hako::zenoh_topology::Inventory> inventory;
        if (options.inventory_path.has_value()) {
            inventory = hako::zenoh_topology::load_inventory(*options.inventory_path);
        }
        std::vector<std::unique_ptr<hako::zenoh_topology::TopologyPduSubscriber>> subscribers;
        std::unique_ptr<hako::zenoh_topology::TopologyPduMultiplexer> multiplexer;
        std::vector<std::optional<hako::zenoh_topology::TopologySnapshot>> latest_observations;
        std::vector<std::optional<std::chrono::steady_clock::time_point>> last_received_times;
        std::vector<std::optional<std::uint64_t>> last_received_at_ms;
        struct DynamicObservation {
            hako::zenoh_topology::TopologySnapshot snapshot;
            std::chrono::steady_clock::time_point last_received_time;
            std::uint64_t last_received_at_ms;
            bool connected{true};
        };
        std::map<std::string, DynamicObservation> dynamic_observations;
        std::map<const hako::zenoh_topology::TopologyPduSubscriber*, std::string>
            dynamic_connection_zids;
        if (inventory.has_value()) {
            latest_observations.resize(inventory->targets.size());
            last_received_times.resize(inventory->targets.size());
            last_received_at_ms.resize(inventory->targets.size());
            if (!inventory->endpoint_mux_config.empty()) {
                multiplexer = std::make_unique<hako::zenoh_topology::TopologyPduMultiplexer>(
                    inventory->endpoint_mux_config);
                multiplexer->start();
            } else {
                subscribers.reserve(inventory->targets.size());
                for (const auto& target : inventory->targets) {
                    auto subscriber = std::make_unique<hako::zenoh_topology::TopologyPduSubscriber>(
                        target.endpoint_config);
                    subscriber->start();
                    subscribers.push_back(std::move(subscriber));
                }
            }
        }
#if defined(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
        std::unique_ptr<hako::zenoh_topology::ZenohCollector> zenoh_collector;
        if (!options.input_file_path.has_value() && !options.inventory_path.has_value()) {
            zc_init_log_from_env_or("error");
            zenoh_collector = std::make_unique<hako::zenoh_topology::ZenohCollector>(options.config_path);
        }
#endif
        const auto collect_json = [&]() {
            if (options.input_file_path.has_value()) {
                return read_text_file(*options.input_file_path);
            }
            if (inventory.has_value()) {
                std::vector<hako::zenoh_topology::TargetObservation> observations;
                std::vector<hako::zenoh_topology::ObservationSource> failures;
                std::vector<std::optional<std::string>> target_errors(inventory->targets.size());

                if (multiplexer) {
                    auto accepted = multiplexer->take_subscribers();
                    for (auto& subscriber : accepted) subscribers.push_back(std::move(subscriber));

                    for (auto it = subscribers.begin(); it != subscribers.end();) {
                        auto* subscriber_ptr = it->get();
                        try {
                            std::optional<std::string> latest_json;
                            while (auto json = (*it)->receive_json()) latest_json = std::move(*json);
                            if (latest_json.has_value()) {
                                auto snapshot = hako::zenoh_topology::topology_from_json(*latest_json);
                                if (snapshot.collector_zid.empty()) {
                                    throw std::runtime_error("received topology without collector zid");
                                }
                                if (inventory->dynamic_targets) {
                                    const auto zid = snapshot.collector_zid;
                                    if (!snapshot.collector_agent_name.empty()) {
                                        for (auto old = dynamic_observations.begin();
                                             old != dynamic_observations.end();) {
                                            if (old->first != zid && !old->second.connected
                                                && old->second.snapshot.collector_agent_name
                                                    == snapshot.collector_agent_name) {
                                                old = dynamic_observations.erase(old);
                                            } else {
                                                ++old;
                                            }
                                        }
                                    }
                                    const auto found = dynamic_observations.find(zid);
                                    if (found == dynamic_observations.end()
                                        || snapshot.timestamp_ms >= found->second.snapshot.timestamp_ms) {
                                        dynamic_observations.insert_or_assign(zid, DynamicObservation{
                                            std::move(snapshot), std::chrono::steady_clock::now(),
                                            system_now_ms(), true});
                                    }
                                    dynamic_connection_zids.insert_or_assign(subscriber_ptr, zid);
                                } else {
                                    const auto target_it = std::find_if(
                                        inventory->targets.begin(), inventory->targets.end(),
                                        [&](const auto& target) {
                                            return target.expected_zid == snapshot.collector_zid;
                                        });
                                    if (target_it == inventory->targets.end()) {
                                        throw std::runtime_error(
                                            "received topology from unknown zid " + snapshot.collector_zid);
                                    }
                                    const auto index = static_cast<std::size_t>(
                                        std::distance(inventory->targets.begin(), target_it));
                                    if (!latest_observations[index].has_value()
                                        || snapshot.timestamp_ms >= latest_observations[index]->timestamp_ms) {
                                        latest_observations[index] = std::move(snapshot);
                                        last_received_times[index] = std::chrono::steady_clock::now();
                                        last_received_at_ms[index] = system_now_ms();
                                    }
                                }
                            }
                        } catch (const std::exception& e) {
                            failures.push_back({"multiplexer-session", "", inventory->endpoint_mux_config,
                                                "", "error", e.what()});
                        }
                        if (!(*it)->is_running()) {
                            if (inventory->dynamic_targets) {
                                const auto connection = dynamic_connection_zids.find(subscriber_ptr);
                                if (connection != dynamic_connection_zids.end()) {
                                    const auto observation = dynamic_observations.find(connection->second);
                                    if (observation != dynamic_observations.end()) {
                                        observation->second.connected = false;
                                    }
                                    dynamic_connection_zids.erase(connection);
                                }
                            }
                            it = subscribers.erase(it);
                        } else {
                            ++it;
                        }
                    }
                } else {
                    for (std::size_t i = 0; i < inventory->targets.size(); ++i) {
                        const auto& target = inventory->targets[i];
                        try {
                            std::optional<std::string> latest_json;
                            while (auto json = subscribers[i]->receive_json()) {
                                latest_json = std::move(*json);
                            }
                            if (latest_json.has_value()) {
                                auto snapshot = hako::zenoh_topology::topology_from_json(*latest_json);
                                if (!target.expected_zid.empty() && snapshot.collector_zid != target.expected_zid) {
                                    throw std::runtime_error("expected zid " + target.expected_zid
                                        + ", received " + snapshot.collector_zid);
                                }
                                latest_observations[i] = std::move(snapshot);
                                last_received_times[i] = std::chrono::steady_clock::now();
                                last_received_at_ms[i] = system_now_ms();
                            }
                        } catch (const std::exception& e) {
                            target_errors[i] = e.what();
                        }
                    }
                }

                if (inventory->dynamic_targets) {
                    for (const auto& [zid, state] : dynamic_observations) {
                        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - state.last_received_time).count();
                        const auto& agent_name = state.snapshot.collector_agent_name;
                        const hako::zenoh_topology::InventoryTarget target{
                            agent_name.empty() ? zid : agent_name,
                            "unknown", inventory->endpoint_mux_config, zid};
                        if (elapsed_ms >= static_cast<std::int64_t>(inventory->stale_after_ms)) {
                            observations.push_back({
                                target,
                                state.snapshot,
                                "stale",
                                "no topology PDU received for " + std::to_string(elapsed_ms) + " ms",
                                state.last_received_at_ms,
                            });
                        } else {
                            observations.push_back({
                                target, state.snapshot, "ok", "", state.last_received_at_ms});
                        }
                    }
                } else {
                    for (std::size_t i = 0; i < inventory->targets.size(); ++i) {
                        const auto& target = inventory->targets[i];
                        if (target_errors[i].has_value()) {
                            failures.push_back({target.name, target.role, target.endpoint_config,
                                                "", "error", *target_errors[i]});
                        } else if (latest_observations[i].has_value()) {
                            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - *last_received_times[i]).count();
                            if (elapsed_ms >= static_cast<std::int64_t>(inventory->stale_after_ms)) {
                                observations.push_back({
                                    target,
                                    *latest_observations[i],
                                    "stale",
                                    "no topology PDU received for " + std::to_string(elapsed_ms) + " ms",
                                    last_received_at_ms[i],
                                });
                            } else {
                                observations.push_back({
                                    target, *latest_observations[i], "ok", "", last_received_at_ms[i]});
                            }
                        } else {
                            failures.push_back({target.name, target.role, target.endpoint_config,
                                                "", "waiting", "no topology PDU received yet"});
                        }
                    }
                }
                return hako::zenoh_topology::to_json(
                    hako::zenoh_topology::aggregate_topology(observations, failures));
            }
#if defined(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
            return hako::zenoh_topology::to_json(zenoh_collector->collect_once());
#else
            throw std::runtime_error(
                "this collector was built without Zenoh; --input-file is required");
#endif
        };

        if (options.endpoint_config_path.has_value()) {
            hako::zenoh_topology::TopologyPduPublisher publisher(*options.endpoint_config_path);
            publisher.start();
            do {
                const auto json = collect_json();
                if (options.print_stdout) {
                    std::cout << json << std::endl;
                }
                publisher.publish_json(json);
                const auto interval = options.publish_interval_ms > 0
                    ? options.publish_interval_ms
                    : (inventory.has_value() ? inventory->refresh_interval_ms : 0);
                if (interval > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(interval));
                }
            } while (options.publish_interval_ms > 0 || inventory.has_value());
        } else {
            const auto json = collect_json();
            if (options.print_stdout) {
                std::cout << json << std::endl;
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "collector error: " << e.what() << std::endl;
        return 1;
    }
}
