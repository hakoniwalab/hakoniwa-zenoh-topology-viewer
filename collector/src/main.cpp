#include <exception>
#include <iostream>
#include <optional>
#include <string>

#include "topology_json.hpp"
#include "zenoh.h"
#include "zenoh_collector.hpp"

namespace {

struct Options {
    std::optional<std::string> config_path;
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
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: hako-zenoh-topology-collector [--config <zenoh.json5>]\n"
                << "\n"
                << "Print one topology snapshot as JSON to stdout.\n";
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
        std::cout << hako::zenoh_topology::to_json(snapshot) << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "collector error: " << e.what() << std::endl;
        return 1;
    }
}
