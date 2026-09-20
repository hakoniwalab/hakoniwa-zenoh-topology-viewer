#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

#include "topology_aggregator.hpp"
#include "topology_json.hpp"

using namespace hako::zenoh_topology;

namespace {

TopologySnapshot observation(
    const std::string& local_zid,
    const std::string& local_mode,
    const std::string& remote_zid,
    const std::string& remote_mode,
    const std::string& src,
    const std::string& dst)
{
    TopologySnapshot snapshot;
    snapshot.collector_zid = local_zid;
    snapshot.nodes = {{local_zid, local_mode}, {remote_zid, remote_mode}};
    TransportInfo transport;
    transport.source_zid = local_zid;
    transport.remote_zid = remote_zid;
    transport.remote_mode = remote_mode;
    snapshot.transports.push_back(transport);
    LinkInfo link;
    link.source_zid = local_zid;
    link.remote_zid = remote_zid;
    link.protocol = "tcp";
    link.src_endpoint = src;
    link.dst_endpoint = dst;
    snapshot.links.push_back(link);
    return snapshot;
}

}  // namespace

int main()
{
    const InventoryTarget router{"router", "router", "router-in.json", "router-zid"};
    const InventoryTarget peer{"peer", "peer", "peer-in.json", "peer-zid"};
    std::vector<TargetObservation> observations = {
        {router, observation("router-zid", "router", "peer-zid", "peer",
                             "tcp/10.0.0.1:7447", "tcp/10.0.0.2:50000")},
        {peer, observation("peer-zid", "peer", "router-zid", "router",
                           "tcp/10.0.0.2:50000", "tcp/10.0.0.1:7447")},
    };

    auto result = aggregate_topology(observations);
    assert(result.schema == "hakoniwa.zenoh.topology/v2");
    assert(result.status == "complete");
    assert(result.nodes.size() == 2);
    assert(result.links.size() == 1);
    assert(result.links.front().observed_by.size() == 2);
    assert(result.sources.size() == 2);

    observations.front().status = "stale";
    observations.front().error = "no topology PDU received for 5000 ms";
    observations.front().last_received_at_ms = 1234;
    result = aggregate_topology(observations);
    assert(result.status == "partial");
    assert(result.links.size() == 1);
    assert(result.sources.front().status == "stale");
    assert(result.sources.front().last_received_at_ms == 1234);

    const auto stale_round_trip = topology_from_json(to_json(result));
    assert(stale_round_trip.sources.size() == 2);
    assert(stale_round_trip.sources.front().status == "stale");
    assert(stale_round_trip.sources.front().last_received_at_ms == 1234);

    const auto round_trip = topology_from_json(to_json(result));
    assert(round_trip.schema == result.schema);
    assert(round_trip.links.size() == 1);
    assert(round_trip.links.front().source_zid == "router-zid");

    ObservationSource failed{"missing", "peer", "missing-in.json", "", "error", "no data"};
    result = aggregate_topology({observations.front()}, {failed});
    assert(result.status == "partial");
    assert(result.sources.size() == 2);
    return 0;
}
