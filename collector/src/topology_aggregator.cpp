#include "topology_aggregator.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace hako::zenoh_topology {
namespace {

std::uint64_t now_ms()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::string read_file(const std::string& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open inventory: " + path);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void merge_node(std::map<std::string, NodeInfo>& nodes, const NodeInfo& candidate)
{
    if (candidate.zid.empty()) return;
    auto [it, inserted] = nodes.emplace(candidate.zid, candidate);
    if (!inserted && (it->second.mode.empty() || it->second.mode == "unknown") && candidate.mode != "unknown") {
        it->second.mode = candidate.mode;
    }
}

std::string link_key(const LinkInfo& link)
{
    auto a = link.source_zid;
    auto b = link.remote_zid;
    if (b < a) std::swap(a, b);
    auto src = link.src_endpoint;
    auto dst = link.dst_endpoint;
    if (dst < src) std::swap(src, dst);
    return a + "\n" + b + "\n" + link.protocol + "\n" + src + "\n" + dst + "\n" + link.group;
}

}  // namespace

Inventory load_inventory(const std::string& path)
{
    try {
        const auto root = nlohmann::json::parse(read_file(path));
        if (root.at("schema") != "hakoniwa.zenoh.inventory/v1") throw std::runtime_error("unsupported inventory schema");
        Inventory result;
        result.refresh_interval_ms = root.value("refresh_interval_ms", std::uint64_t{1000});
        if (result.refresh_interval_ms == 0) throw std::runtime_error("refresh_interval_ms must be positive");
        result.stale_after_ms = root.value("stale_after_ms", std::uint64_t{5000});
        if (result.stale_after_ms == 0) throw std::runtime_error("stale_after_ms must be positive");
        const auto base_dir = std::filesystem::absolute(path).parent_path();
        if (root.contains("endpoint_mux_config")) {
            auto mux_config = std::filesystem::path(root.at("endpoint_mux_config").get<std::string>());
            if (mux_config.is_relative()) mux_config = base_dir / mux_config;
            result.endpoint_mux_config = mux_config.lexically_normal().string();
        }
        result.dynamic_targets = root.value("dynamic_targets", false);
        const auto targets = root.value("targets", nlohmann::json::array());
        for (const auto& value : targets) {
            InventoryTarget target;
            target.name = value.at("name").get<std::string>();
            target.role = value.at("role").get<std::string>();
            if (value.contains("endpoint_config")) {
                auto endpoint_config = std::filesystem::path(value.at("endpoint_config").get<std::string>());
                if (endpoint_config.is_relative()) endpoint_config = base_dir / endpoint_config;
                target.endpoint_config = endpoint_config.lexically_normal().string();
            } else {
                target.endpoint_config = result.endpoint_mux_config;
            }
            target.expected_zid = value.value("expected_zid", "");
            if (target.name.empty() || target.endpoint_config.empty()) throw std::runtime_error("inventory target name/endpoint_config must not be empty");
            if (!result.endpoint_mux_config.empty() && target.expected_zid.empty()) {
                throw std::runtime_error("multiplexed inventory targets require expected_zid");
            }
            result.targets.push_back(std::move(target));
        }
        if (result.dynamic_targets) {
            if (result.endpoint_mux_config.empty()) {
                throw std::runtime_error("dynamic inventory requires endpoint_mux_config");
            }
            if (!result.targets.empty()) {
                throw std::runtime_error("dynamic inventory must not define fixed targets");
            }
        } else if (result.targets.empty()) {
            throw std::runtime_error("inventory must contain at least one target");
        }
        return result;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("invalid inventory JSON: ") + e.what());
    }
}

TopologySnapshot aggregate_topology(
    const std::vector<TargetObservation>& observations,
    const std::vector<ObservationSource>& failed_sources)
{
    TopologySnapshot result;
    result.schema = "hakoniwa.zenoh.topology/v2";
    result.timestamp_ms = now_ms();
    result.status = failed_sources.empty() ? "complete" : "partial";
    result.sources = failed_sources;

    std::map<std::string, NodeInfo> nodes;
    std::set<std::tuple<std::string, std::string, std::string, bool, bool>> transport_keys;
    std::map<std::string, LinkInfo> links;

    for (const auto& observation : observations) {
        const auto& snapshot = observation.snapshot;
        if (!observation.target.expected_zid.empty() && snapshot.collector_zid != observation.target.expected_zid) {
            throw std::runtime_error("target " + observation.target.name + " returned unexpected zid " + snapshot.collector_zid);
        }
        result.sources.push_back({observation.target.name, observation.target.role, observation.target.endpoint_config,
                                  snapshot.collector_zid, observation.status, observation.error,
                                  observation.last_received_at_ms});
        if (observation.status != "ok") result.status = "partial";
        merge_node(nodes, {snapshot.collector_zid, observation.target.role});
        for (const auto& node : snapshot.nodes) merge_node(nodes, node);

        for (auto transport : snapshot.transports) {
            if (transport.source_zid.empty()) transport.source_zid = snapshot.collector_zid;
            const auto key = std::make_tuple(transport.source_zid, transport.remote_zid,
                                             transport.remote_mode, transport.qos, transport.multicast);
            if (transport_keys.insert(key).second) result.transports.push_back(std::move(transport));
        }
        for (auto link : snapshot.links) {
            if (link.source_zid.empty()) link.source_zid = snapshot.collector_zid;
            if (link.observed_by.empty()) link.observed_by.push_back(snapshot.collector_zid);
            const auto key = link_key(link);
            auto [it, inserted] = links.emplace(key, link);
            if (!inserted) {
                for (const auto& observer : link.observed_by) {
                    if (std::find(it->second.observed_by.begin(), it->second.observed_by.end(), observer) == it->second.observed_by.end()) {
                        it->second.observed_by.push_back(observer);
                    }
                }
            }
        }
    }

    for (const auto& [key, node] : nodes) result.nodes.push_back(node);
    for (auto& [key, link] : links) {
        std::sort(link.observed_by.begin(), link.observed_by.end());
        result.links.push_back(std::move(link));
    }
    std::sort(result.transports.begin(), result.transports.end(), [](const auto& a, const auto& b) {
        return std::tie(a.source_zid, a.remote_zid) < std::tie(b.source_zid, b.remote_zid);
    });
    return result;
}

}  // namespace hako::zenoh_topology
