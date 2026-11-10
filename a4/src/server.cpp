#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include "cdma.hpp" 

using namespace cdma;

std::atomic<bool> g_server_running(true);
CdmaChannelServer* g_server_ptr = nullptr;

void sigint_handler(int /*signum*/) {
    std::cout << "\n[Server] Interrupted by user. Shutting down gracefully...\n";
    g_server_running = false;
    if (g_server_ptr) {
        g_server_ptr->stop();
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -P, --port <port>         TCP port for common channel (default: 9090)\n"
              << "  -N, --stations <num>      Total number of stations N (default: 4)\n"
              << "  --noise <stddev>          Simulated Gaussian channel noise stddev (default: 0.0)\n"
              << "  -b, --bind <ip>           Bind IP address (default: 0.0.0.0)\n"
              << "  -q, --quiet               Quiet mode (suppress per-round verbose logs)\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    ChannelConfig config;
    config.port = 9090;
    config.num_stations = 4;
    config.noise_stddev = 0.0;
    config.bind_ip = "0.0.0.0";
    config.verbose = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-P" || arg == "--port") {
            if (i + 1 < argc) config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-N" || arg == "--stations") {
            if (i + 1 < argc) config.num_stations = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--noise") {
            if (i + 1 < argc) config.noise_stddev = std::stod(argv[++i]);
        } else if (arg == "-b" || arg == "--bind") {
            if (i + 1 < argc) config.bind_ip = argv[++i];
        } else if (arg == "-q" || arg == "--quiet") {
            config.verbose = false;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    try {
        CdmaChannelServer server(config);
        g_server_ptr = &server;
        if (!server.run()) {
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal Error] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
