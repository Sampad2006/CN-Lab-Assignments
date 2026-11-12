#include "virtual_switch.hpp"
#include "logger.hpp"
#include <iostream>
#include <csignal>

static std::atomic<bool> g_stop{false};

void handle_signal(int sig) {
    (void)sig;
    g_stop = true;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8000;
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    arp::VirtualSwitch sw(port);
    if (!sw.start()) {
        return 1;
    }

    std::cout << "\n========================================================\n"
              << " Virtual L2 Ethernet Switch Daemon running on port " << port << "\n"
              << " Press Ctrl+C or type 'cam' / 'status' / 'quit' in terminal.\n"
              << "========================================================\n\n";

    std::string line;
    while (!g_stop && std::getline(std::cin, line)) {
        if (line == "cam" || line == "show mac" || line == "show mac address-table") {
            sw.print_cam_table(std::cout);
        } else if (line == "status") {
            std::cout << "Switch active ports: " << sw.active_ports_count() << "\n";
        } else if (line == "quit" || line == "exit") {
            break;
        } else if (!line.empty()) {
            std::cout << "Commands: 'cam' (show CAM table), 'status', 'quit'\n";
        }
    }

    sw.stop();
    return 0;
}
