#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <cstdlib>
#include "cdma.hpp"

using namespace cdma;

CdmaStationClient* g_station_ptr = nullptr;

void sigint_handler(int /*signum*/) {
    std::cout << "\n[Station] Disconnecting from channel...\n";
    if (g_station_ptr) {  
        g_station_ptr->close();
    }
    std::exit(0);
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -H, --host <ip>           Channel server IP (default: 127.0.0.1)\n"
              << "  -P, --port <port>         Channel server TCP port (default: 9090)\n"
              << "  -m, --msg <text>          Automated message transmission mode\n"
              << "  -b, --bit <1|0|s>         Automated single-bit transmission mode\n"
              << "  -q, --quiet               Quiet mode\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 9090;
    std::string msg_to_send = "";
    std::string bit_to_send = "";
    bool verbose = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-H" || arg == "--host") {
            if (i + 1 < argc) host = argv[++i];
        } else if (arg == "-P" || arg == "--port") {
            if (i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-m" || arg == "--msg") {
            if (i + 1 < argc) msg_to_send = argv[++i];
        } else if (arg == "-b" || arg == "--bit") {
            if (i + 1 < argc) bit_to_send = argv[++i];
        } else if (arg == "-q" || arg == "--quiet") {
            verbose = false;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    CdmaStationClient station(host, port, verbose);
    g_station_ptr = &station;

    if (!station.connect_and_register()) {
        return 1;
    }

    if (!msg_to_send.empty()) {
        std::cout << "\n[Message Mode] Transmitting string: \"" << msg_to_send << "\" over CDMA channel...\n";
        auto recovered = station.transmit_message(msg_to_send);
        std::cout << "\n=======================================================\n";
        std::cout << "  Simultaneous CDMA Message Reconstruction at Station " << station.get_station_id() << ":\n";
        std::cout << "=======================================================\n";
        for (const auto& pair : recovered) {
            std::cout << "  Station " << pair.first << ": \"" << pair.second << "\"\n";
        }
    } else if (!bit_to_send.empty()) {
        int b = -1;
        if (bit_to_send == "1") b = 1;
        else if (bit_to_send == "0") b = 0;
        std::vector<WalshMatrix::DecodeResult> results;
        if (station.transmit_bit_round(b, results)) {
            std::cout << "\n[Automated Bit Mode Result for Station " << station.get_station_id() << "]:\n";
            for (size_t i = 0; i < results.size(); ++i) {
                std::cout << "  Station " << (i + 1) << " => " << results[i].description
                          << " (DotProduct: " << results[i].dot_product
                          << ", Norm: " << results[i].normalized << ")\n";
            }
        }
    } else {
        std::cout << "\nEntering interactive round loop. Press Ctrl+C or 'q' to quit.\n";
        station.run_interactive_session();
    }

    return 0;
}
