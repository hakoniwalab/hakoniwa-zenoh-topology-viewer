#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "topology_json.hpp"
#include "topology_pdu_publisher.hpp"
#if defined(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
#include "zenoh.h"
#include "zenoh_collector.hpp"
#endif

namespace {

struct Options {
    std::optional<std::string> config_path;
    std::optional<std::string> input_file_path;
    std::optional<std::string> endpoint_config_path;
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
                << "      --publish-interval-ms <ms>   Re-read and publish the input file periodically\n"
                << "      --no-stdout                  Do not print the JSON snapshot\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (options.config_path.has_value() && options.input_file_path.has_value()) {
        throw std::runtime_error("--config and --input-file are mutually exclusive");
    }
    if (options.publish_interval_ms > 0 && !options.input_file_path.has_value()) {
        throw std::runtime_error("--publish-interval-ms requires --input-file");
    }
    return options;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        const auto options = parse_args(argc, argv);
        const auto collect_json = [&options]() {
            if (options.input_file_path.has_value()) {
                return read_text_file(*options.input_file_path);
            }
#if defined(HAKO_ZENOH_TOPOLOGY_WITH_ZENOH)
            zc_init_log_from_env_or("error");
            const hako::zenoh_topology::ZenohCollector collector(options.config_path);
            return hako::zenoh_topology::to_json(collector.collect_once());
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
                if (options.publish_interval_ms > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(options.publish_interval_ms));
                }
            } while (options.publish_interval_ms > 0);
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
