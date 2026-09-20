#include "zenoh_collector.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

#include "zenoh.h"

namespace hako::zenoh_topology {
namespace {

std::string owned_string_to_std(z_owned_string_t& value)
{
    const auto* loaned = z_loan(value);
    return std::string(z_string_data(loaned), z_string_len(loaned));
}

std::string zid_to_string(const z_id_t& zid)
{
    z_owned_string_t value;
    z_id_to_string(&zid, &value);
    auto result = owned_string_to_std(value);
    z_drop(z_move(value));
    return result;
}

std::string whatami_to_string(z_whatami_t whatami)
{
    z_view_string_t value;
    z_whatami_to_view_string(whatami, &value);
    const auto* loaned = z_loan(value);
    return std::string(z_string_data(loaned), z_string_len(loaned));
}

std::string endpoint_protocol(const std::string& endpoint)
{
    const auto pos = endpoint.find('/');
    if (pos == std::string::npos) {
        return {};
    }
    return endpoint.substr(0, pos);
}

void add_or_update_node(TopologySnapshot& snapshot, std::string zid, std::string mode)
{
    const auto it = std::find_if(
        snapshot.nodes.begin(),
        snapshot.nodes.end(),
        [&zid](const NodeInfo& node) { return node.zid == zid; });

    if (it == snapshot.nodes.end()) {
        snapshot.nodes.push_back(NodeInfo{std::move(zid), std::move(mode)});
        return;
    }

    if (it->mode == "unknown" && mode != "unknown") {
        it->mode = std::move(mode);
    }
}

struct NodeCallbackContext {
    TopologySnapshot* snapshot;
    const char* mode;
};

void collect_node_id(const z_id_t* zid, void* ctx)
{
    auto& context = *static_cast<NodeCallbackContext*>(ctx);
    add_or_update_node(*context.snapshot, zid_to_string(*zid), context.mode);
}

void collect_transport(z_loaned_transport_t* transport, void* ctx)
{
    auto& snapshot = *static_cast<TopologySnapshot*>(ctx);

    TransportInfo info;
    info.source_zid = snapshot.collector_zid;
    info.remote_zid = zid_to_string(z_transport_zid(transport));
    info.remote_mode = whatami_to_string(z_transport_whatami(transport));
    info.qos = z_transport_is_qos(transport);
    info.multicast = z_transport_is_multicast(transport);
#if defined(Z_FEATURE_SHARED_MEMORY)
    info.shm = z_transport_is_shm(transport);
#endif

    add_or_update_node(snapshot, info.remote_zid, info.remote_mode);
    snapshot.transports.push_back(std::move(info));
}

void collect_link(z_loaned_link_t* link, void* ctx)
{
    auto& snapshot = *static_cast<TopologySnapshot*>(ctx);

    LinkInfo info;
    info.source_zid = snapshot.collector_zid;
    info.remote_zid = zid_to_string(z_link_zid(link));
    info.observed_by.push_back(snapshot.collector_zid);

    z_owned_string_t src;
    z_owned_string_t dst;
    z_owned_string_t group;
    z_owned_string_t auth_id;
    z_link_src(link, &src);
    z_link_dst(link, &dst);
    z_link_group(link, &group);
    z_link_auth_identifier(link, &auth_id);

    info.src_endpoint = owned_string_to_std(src);
    info.dst_endpoint = owned_string_to_std(dst);
    info.group = owned_string_to_std(group);
    info.auth_id = owned_string_to_std(auth_id);
    info.protocol = endpoint_protocol(info.dst_endpoint);
    if (info.protocol.empty()) {
        info.protocol = endpoint_protocol(info.src_endpoint);
    }

    info.mtu = z_link_mtu(link);
    info.streamed = z_link_is_streamed(link);

    z_owned_string_array_t interfaces;
    z_link_interfaces(link, &interfaces);
    const auto interfaces_len = z_string_array_len(z_loan(interfaces));
    for (std::size_t i = 0; i < interfaces_len; ++i) {
        const auto* iface = z_string_array_get(z_loan(interfaces), i);
        info.interfaces.emplace_back(z_string_data(iface), z_string_len(iface));
    }

    std::uint8_t min_priority = 0;
    std::uint8_t max_priority = 0;
    if (z_link_priorities(link, &min_priority, &max_priority)) {
        info.min_priority = min_priority;
        info.max_priority = max_priority;
    }

    z_reliability_t reliability;
    if (z_link_reliability(link, &reliability)) {
        info.reliability =
            reliability == Z_RELIABILITY_RELIABLE ? "reliable" : "best_effort";
    }

    z_drop(z_move(interfaces));
    z_drop(z_move(src));
    z_drop(z_move(dst));
    z_drop(z_move(group));
    z_drop(z_move(auth_id));

    add_or_update_node(snapshot, info.remote_zid, "unknown");
    snapshot.links.push_back(std::move(info));
}

std::uint64_t now_ms()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace

struct ZenohCollector::Impl {
    z_owned_session_t session;
};

ZenohCollector::ZenohCollector(std::optional<std::string> config_path)
    : impl_(std::make_unique<Impl>())
{
    z_owned_config_t config;
    if (config_path.has_value()) {
        if (zc_config_from_file(&config, config_path->c_str()) < 0) {
            throw std::runtime_error("failed to load Zenoh config: " + *config_path);
        }
    } else {
        z_config_default(&config);
    }

    if (z_open(&impl_->session, z_move(config), nullptr) < 0) {
        throw std::runtime_error("failed to open Zenoh session");
    }
}

ZenohCollector::~ZenohCollector()
{
    if (impl_) z_drop(z_move(impl_->session));
}

TopologySnapshot ZenohCollector::collect_once() const
{
    return collect_session_topology(z_loan(impl_->session));
}

TopologySnapshot collect_session_topology(const z_loaned_session_t* session)
{

    TopologySnapshot snapshot;
    snapshot.timestamp_ms = now_ms();
    snapshot.collector_zid = zid_to_string(z_info_zid(session));
    add_or_update_node(snapshot, snapshot.collector_zid, "unknown");

    NodeCallbackContext router_context{&snapshot, "router"};
    z_owned_closure_zid_t router_callback;
    z_closure(&router_callback, collect_node_id, nullptr, &router_context);
    if (z_info_routers_zid(session, z_move(router_callback)) < 0) {
        throw std::runtime_error("failed to query connected Zenoh routers");
    }

    NodeCallbackContext peer_context{&snapshot, "peer"};
    z_owned_closure_zid_t peer_callback;
    z_closure(&peer_callback, collect_node_id, nullptr, &peer_context);
    if (z_info_peers_zid(session, z_move(peer_callback)) < 0) {
        throw std::runtime_error("failed to query connected Zenoh peers");
    }

    z_owned_closure_transport_t transport_callback;
    z_closure(&transport_callback, collect_transport, nullptr, &snapshot);
    if (z_info_transports(session, z_move(transport_callback)) < 0) {
        throw std::runtime_error("failed to query Zenoh transports");
    }

    z_owned_closure_link_t link_callback;
    z_closure(&link_callback, collect_link, nullptr, &snapshot);
    if (z_info_links(session, z_move(link_callback), nullptr) < 0) {
        throw std::runtime_error("failed to query Zenoh links");
    }

    return snapshot;
}

}  // namespace hako::zenoh_topology
