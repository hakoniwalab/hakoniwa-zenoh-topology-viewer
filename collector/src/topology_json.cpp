#include "topology_json.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <utility>

namespace hako::zenoh_topology {
namespace {

using json = nlohmann::json;

json node_json(const NodeInfo& value)
{
    return {{"zid", value.zid}, {"mode", value.mode}};
}

json transport_json(const TransportInfo& value)
{
    json result = {
        {"remote_zid", value.remote_zid},
        {"remote_mode", value.remote_mode},
        {"qos", value.qos},
        {"multicast", value.multicast},
    };
    if (!value.source_zid.empty()) result["source_zid"] = value.source_zid;
    if (value.shm.has_value()) result["shm"] = *value.shm;
    return result;
}

json link_json(const LinkInfo& value)
{
    json result = {
        {"remote_zid", value.remote_zid},
        {"protocol", value.protocol},
        {"src_endpoint", value.src_endpoint},
        {"dst_endpoint", value.dst_endpoint},
        {"mtu", value.mtu},
        {"streamed", value.streamed},
        {"interfaces", value.interfaces},
    };
    if (!value.source_zid.empty()) result["source_zid"] = value.source_zid;
    if (!value.group.empty()) result["group"] = value.group;
    if (!value.auth_id.empty()) result["auth_id"] = value.auth_id;
    if (value.min_priority.has_value() && value.max_priority.has_value()) {
        result["priority"] = {{"min", *value.min_priority}, {"max", *value.max_priority}};
    }
    if (value.reliability.has_value()) result["reliability"] = *value.reliability;
    if (!value.observed_by.empty()) result["observed_by"] = value.observed_by;
    return result;
}

json source_json(const ObservationSource& value)
{
    json result = {
        {"name", value.name}, {"role", value.role}, {"endpoint", value.endpoint},
        {"zid", value.zid}, {"status", value.status},
    };
    if (!value.error.empty()) result["error"] = value.error;
    if (value.last_received_at_ms.has_value()) {
        result["last_received_at"] = *value.last_received_at_ms;
    }
    return result;
}

template <typename T>
std::optional<T> optional_value(const json& value, const char* key)
{
    if (!value.contains(key) || value.at(key).is_null()) return std::nullopt;
    return value.at(key).get<T>();
}

}  // namespace

std::string to_json(const TopologySnapshot& snapshot)
{
    json root = {
        {"schema", snapshot.schema},
        {"timestamp", snapshot.timestamp_ms},
        {"collector", {{"zid", snapshot.collector_zid}}},
        {"status", snapshot.status},
        {"nodes", json::array()},
        {"transports", json::array()},
        {"links", json::array()},
    };
    if (!snapshot.collector_agent_id.empty()) {
        root["collector"]["agent_id"] = snapshot.collector_agent_id;
    }
    for (const auto& node : snapshot.nodes) root["nodes"].push_back(node_json(node));
    for (const auto& transport : snapshot.transports) root["transports"].push_back(transport_json(transport));
    for (const auto& link : snapshot.links) root["links"].push_back(link_json(link));
    if (!snapshot.sources.empty()) {
        root["sources"] = json::array();
        for (const auto& source : snapshot.sources) root["sources"].push_back(source_json(source));
    }
    return root.dump();
}

TopologySnapshot topology_from_json(const std::string& input)
{
    try {
        const auto root = json::parse(input);
        TopologySnapshot snapshot;
        snapshot.schema = root.at("schema").get<std::string>();
        snapshot.timestamp_ms = root.value("timestamp", std::uint64_t{0});
        if (root.contains("collector")) {
            snapshot.collector_zid = root.at("collector").value("zid", "");
            snapshot.collector_agent_id = root.at("collector").value("agent_id", "");
        }
        snapshot.status = root.value("status", "complete");

        for (const auto& value : root.value("sources", json::array())) {
            ObservationSource source;
            source.name = value.at("name").get<std::string>();
            source.role = value.value("role", "");
            source.endpoint = value.value("endpoint", "");
            source.zid = value.value("zid", "");
            source.status = value.value("status", "ok");
            source.error = value.value("error", "");
            source.last_received_at_ms = optional_value<std::uint64_t>(value, "last_received_at");
            snapshot.sources.push_back(std::move(source));
        }

        for (const auto& value : root.value("nodes", json::array())) {
            snapshot.nodes.push_back({value.at("zid").get<std::string>(), value.value("mode", "unknown")});
        }
        for (const auto& value : root.value("transports", json::array())) {
            TransportInfo item;
            item.source_zid = value.value("source_zid", snapshot.collector_zid);
            item.remote_zid = value.at("remote_zid").get<std::string>();
            item.remote_mode = value.value("remote_mode", "unknown");
            item.qos = value.value("qos", false);
            item.multicast = value.value("multicast", false);
            item.shm = optional_value<bool>(value, "shm");
            snapshot.transports.push_back(std::move(item));
        }
        for (const auto& value : root.value("links", json::array())) {
            LinkInfo item;
            item.source_zid = value.value("source_zid", snapshot.collector_zid);
            item.remote_zid = value.at("remote_zid").get<std::string>();
            item.protocol = value.value("protocol", "");
            item.src_endpoint = value.value("src_endpoint", "");
            item.dst_endpoint = value.value("dst_endpoint", "");
            item.group = value.value("group", "");
            item.mtu = value.value("mtu", std::uint16_t{0});
            item.streamed = value.value("streamed", false);
            item.interfaces = value.value("interfaces", std::vector<std::string>{});
            item.auth_id = value.value("auth_id", "");
            if (value.contains("priority")) {
                item.min_priority = value.at("priority").at("min").get<std::uint8_t>();
                item.max_priority = value.at("priority").at("max").get<std::uint8_t>();
            }
            item.reliability = optional_value<std::string>(value, "reliability");
            item.observed_by = value.value("observed_by", std::vector<std::string>{});
            snapshot.links.push_back(std::move(item));
        }
        return snapshot;
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("invalid topology JSON: ") + e.what());
    }
}

}  // namespace hako::zenoh_topology
