#include "topology_json.hpp"

#include <iomanip>
#include <sstream>
#include <string_view>

namespace hako::zenoh_topology {
namespace {

std::string escape_json(std::string_view input)
{
    std::ostringstream out;
    for (const unsigned char ch : input) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (ch < 0x20) {
                out << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch)
                    << std::dec << std::setw(0);
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

void write_string(std::ostringstream& out, std::string_view value)
{
    out << '"' << escape_json(value) << '"';
}

void write_nodes(std::ostringstream& out, const std::vector<NodeInfo>& nodes)
{
    out << "[";
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{\"zid\":";
        write_string(out, nodes[i].zid);
        out << ",\"mode\":";
        write_string(out, nodes[i].mode);
        out << "}";
    }
    out << "]";
}

void write_transports(std::ostringstream& out, const std::vector<TransportInfo>& transports)
{
    out << "[";
    for (std::size_t i = 0; i < transports.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        const auto& transport = transports[i];
        out << "{\"remote_zid\":";
        write_string(out, transport.remote_zid);
        out << ",\"remote_mode\":";
        write_string(out, transport.remote_mode);
        out << ",\"qos\":" << (transport.qos ? "true" : "false");
        out << ",\"multicast\":" << (transport.multicast ? "true" : "false");
        if (transport.shm.has_value()) {
            out << ",\"shm\":" << (*transport.shm ? "true" : "false");
        }
        out << "}";
    }
    out << "]";
}

void write_links(std::ostringstream& out, const std::vector<LinkInfo>& links)
{
    out << "[";
    for (std::size_t i = 0; i < links.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        const auto& link = links[i];
        out << "{\"remote_zid\":";
        write_string(out, link.remote_zid);
        out << ",\"protocol\":";
        write_string(out, link.protocol);
        out << ",\"src_endpoint\":";
        write_string(out, link.src_endpoint);
        out << ",\"dst_endpoint\":";
        write_string(out, link.dst_endpoint);
        if (!link.group.empty()) {
            out << ",\"group\":";
            write_string(out, link.group);
        }
        out << ",\"mtu\":" << link.mtu;
        out << ",\"streamed\":" << (link.streamed ? "true" : "false");

        out << ",\"interfaces\":[";
        for (std::size_t j = 0; j < link.interfaces.size(); ++j) {
            if (j != 0) {
                out << ",";
            }
            write_string(out, link.interfaces[j]);
        }
        out << "]";

        if (!link.auth_id.empty()) {
            out << ",\"auth_id\":";
            write_string(out, link.auth_id);
        }
        if (link.min_priority.has_value() && link.max_priority.has_value()) {
            out << ",\"priority\":{\"min\":"
                << static_cast<unsigned>(*link.min_priority)
                << ",\"max\":"
                << static_cast<unsigned>(*link.max_priority)
                << "}";
        }
        if (link.reliability.has_value()) {
            out << ",\"reliability\":";
            write_string(out, *link.reliability);
        }
        out << "}";
    }
    out << "]";
}

}  // namespace

std::string to_json(const TopologySnapshot& snapshot)
{
    std::ostringstream out;
    out << "{\"schema\":";
    write_string(out, snapshot.schema);
    out << ",\"timestamp\":" << snapshot.timestamp_ms;
    out << ",\"collector\":{\"zid\":";
    write_string(out, snapshot.collector_zid);
    out << "},\"nodes\":";
    write_nodes(out, snapshot.nodes);
    out << ",\"transports\":";
    write_transports(out, snapshot.transports);
    out << ",\"links\":";
    write_links(out, snapshot.links);
    out << "}";
    return out.str();
}

}  // namespace hako::zenoh_topology
