#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "topology_json.hpp"
#include "topology_pdu_publisher.hpp"
#include "zenoh.h"
#include "zenoh_collector.hpp"

namespace {

struct Options {
    std::optional<std::string> config_path;
    std::optional<std::string> endpoint_config_path;
    bool print_stdout{true};
};

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
        } else if (arg == "--endpoint-config") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--endpoint-config requires a path");
            }
            options.endpoint_config_path = argv[++i];
        } else if (arg == "--no-stdout") {
            options.print_stdout = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: hako-zenoh-topology-collector [OPTIONS]\n\n"
                << "Collect one Zenoh topology snapshot.\n\n"
                << "Options:\n"
                << "  -c, --config <zenoh.json5>       Zenoh session config\n"
                << "      --endpoint-config <json>     Hakoniwa Endpoint config; publishes CDR std_msgs/String\n"
                << "      --no-stdout                  Do not print the JSON snapshot\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        zc_init_log_from_env_or("error");
        const auto options = parse_args(argc, argv);

        const hako::zenoh_topology::ZenohCollector collector(options.config_path);
        const auto snapshot = collector.collect_once();
        const auto json = hako::zenoh_topology::to_json(snapshot);

        if (options.print_stdout) {
            std::cout << json << std::endl;
        }

        if (options.endpoint_config_path.has_value()) {
            hako::zenoh_topology::TopologyPduPublisher publisher(*options.endpoint_config_path);
            publisher.start();
            publisher.publish_json(json);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "collector error: " << e.what() << std::endl;
        return 1;
    }
}
