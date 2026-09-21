#include <cassert>
#include <string>

#include "topology_json.hpp"

using hako::zenoh_topology::LinkInfo;
using hako::zenoh_topology::NodeInfo;
using hako::zenoh_topology::TopologySnapshot;
using hako::zenoh_topology::TransportInfo;
using hako::zenoh_topology::to_json;

int main()
{
    TopologySnapshot snapshot;
    snapshot.timestamp_ms = 1234;
    snapshot.collector_zid = "collector-\"a";
    snapshot.collector_agent_name = "client-a";
    snapshot.nodes = {
        NodeInfo{"peer-a", "peer"},
        NodeInfo{"router-r", "router"},
    };

    TransportInfo transport;
    transport.remote_zid = "router-r";
    transport.remote_mode = "router";
    transport.qos = true;
    transport.multicast = false;
    snapshot.transports.push_back(transport);

    LinkInfo link;
    link.remote_zid = "router-r";
    link.protocol = "tcp";
    link.src_endpoint = "tcp/127.0.0.1:10000";
    link.dst_endpoint = "tcp/127.0.0.1:7447";
    link.mtu = 65535;
    link.streamed = true;
    link.interfaces = {"lo"};
    link.reliability = "reliable";
    snapshot.links.push_back(link);

    const std::string json = to_json(snapshot);

    assert(json.find("\"schema\":\"hakoniwa.zenoh.topology/v1\"") != std::string::npos);
    assert(json.find("\"timestamp\":1234") != std::string::npos);
    assert(json.find("collector-\\\"a") != std::string::npos);
    assert(json.find("\"name\":\"client-a\"") != std::string::npos);
    assert(json.find("\"mode\":\"peer\"") != std::string::npos);
    assert(json.find("\"protocol\":\"tcp\"") != std::string::npos);
    assert(json.find("\"reliability\":\"reliable\"") != std::string::npos);

    const auto restored = hako::zenoh_topology::topology_from_json(json);
    assert(restored.collector_agent_name == "client-a");

    return 0;
}
