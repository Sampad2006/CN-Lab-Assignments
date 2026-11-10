#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "csma.hpp"

using namespace csma;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -i, --id <num>            Station ID (default: 1)\n"
              << "  -p, --protocol <proto>    CSMA Protocol: non_persistent | 1_persistent | p_persistent | csma_cd (default: non_persistent)\n"
              << "  --prob <val>              p value for p-persistent CSMA (0.01 - 1.0, default: 0.1)\n"
              << "  -n, --frames <num>        Number of frames to transmit (default: 20)\n"
              << "  -s, --size <bytes>        Frame payload size in bytes (default: 64)\n"
              << "  --slot <ms>               Slot time tau in ms (default: 2.0)\n"
              << "  --txtime <ms>             Frame transmission time in ms (default: 10.0)\n"
              << "  --jamtime <ms>            Jam signal time in ms (default: 2.0)\n"
              << "  --interval <ms>           Inter-packet arrival interval in ms (default: 0.0)\n"
              << "  -H, --host <ip>           Channel server IP (default: 127.0.0.1)\n"
              << "  -P, --port <port>         Channel server UDP port (default: 9999)\n"
              << "  -q, --quiet               Quiet mode (suppress per-frame logs)\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    CsmaConfig cfg;
    bool interactive = (argc <= 1);

    if (interactive) {
        std::cout << "========================================================\n"
                  << "    CN Lab Assignment 3: CSMA Contending Station        \n"
                  << "========================================================\n"
                  << "Select CSMA Protocol:\n"
                  << "  1. Non-Persistent CSMA\n"
                  << "  2. 1-Persistent CSMA\n"
                  << "  3. p-Persistent CSMA\n"
                  << "  4. CSMA with Collision Detection (CSMA/CD)\n"
                  << "Enter choice (1-4) [default 1]: ";
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "2") cfg.protocol = ProtocolType::ONE_PERSISTENT;
        else if (choice == "3") {
            cfg.protocol = ProtocolType::P_PERSISTENT;
            std::cout << "Enter probability p (0.01 - 1.0) [default 0.1]: ";
            std::string p_str;
            std::getline(std::cin, p_str);
            if (!p_str.empty()) cfg.p_value = std::stod(p_str);
        } else if (choice == "4") cfg.protocol = ProtocolType::CSMA_CD;
        else cfg.protocol = ProtocolType::NON_PERSISTENT;

        std::cout << "Enter Station ID [default 1]: ";
        std::string id_str;
        std::getline(std::cin, id_str);
        if (!id_str.empty()) cfg.station_id = static_cast<uint32_t>(std::stoi(id_str));

        std::cout << "Enter number of frames to transmit [default 20]: ";
        std::string f_str;
        std::getline(std::cin, f_str);
        if (!f_str.empty()) cfg.total_frames = std::stoi(f_str);
    } else {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-i" || arg == "--id") && i + 1 < argc) {
                cfg.station_id = static_cast<uint32_t>(std::stoi(argv[++i]));
            } else if ((arg == "-p" || arg == "--protocol") && i + 1 < argc) {
                std::string proto = argv[++i];
                if (proto == "non_persistent" || proto == "non-persistent" || proto == "np") {
                    cfg.protocol = ProtocolType::NON_PERSISTENT;
                } else if (proto == "1_persistent" || proto == "1-persistent" || proto == "1p") {
                    cfg.protocol = ProtocolType::ONE_PERSISTENT;
                } else if (proto == "p_persistent" || proto == "p-persistent" || proto == "pp") {
                    cfg.protocol = ProtocolType::P_PERSISTENT;
                } else if (proto == "csma_cd" || proto == "csma-cd" || proto == "cd") {
                    cfg.protocol = ProtocolType::CSMA_CD;
                }
            } else if (arg == "--prob" && i + 1 < argc) {
                cfg.p_value = std::stod(argv[++i]);
            } else if ((arg == "-n" || arg == "--frames") && i + 1 < argc) {
                cfg.total_frames = std::stoi(argv[++i]);
            } else if ((arg == "-s" || arg == "--size") && i + 1 < argc) {
                cfg.payload_size = std::stoi(argv[++i]);
            } else if (arg == "--slot" && i + 1 < argc) {
                cfg.slot_time_ms = std::stod(argv[++i]);
            } else if (arg == "--txtime" && i + 1 < argc) {
                cfg.tx_time_ms = std::stod(argv[++i]);
            } else if (arg == "--jamtime" && i + 1 < argc) {
                cfg.jam_time_ms = std::stod(argv[++i]);
            } else if (arg == "--interval" && i + 1 < argc) {
                cfg.packet_interval_ms = std::stod(argv[++i]);
            } else if ((arg == "-H" || arg == "--host") && i + 1 < argc) {
                cfg.server_ip = argv[++i];
            } else if ((arg == "-P" || arg == "--port") && i + 1 < argc) {
                cfg.server_port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else if (arg == "-q" || arg == "--quiet") {
                cfg.verbose = false;
            } else if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
        }
    }

    CsmaStation station(cfg);
    if (!station.init()) {
        std::cerr << "Failed to initialize station.\n";
        return 1;
    }

    if (cfg.verbose) {
        std::cout << "Starting Station " << cfg.station_id << " (" << protocol_type_to_str(cfg.protocol)
                  << ") -> " << cfg.total_frames << " frames...\n";
    }

    station.run();
    station.print_summary();

    return 0;
}
