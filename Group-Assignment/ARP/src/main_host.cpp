#include "host_node.hpp"
#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <vector>

void print_help() {
    std::cout << "\n--- Available Host CLI Commands ---\n"
              << "  show arp | arp -a       : Display local ARP cache (IP -> MAC table)\n"
              << "  send <ip> <message>     : Send payload to target IP (triggers ARP if needed)\n"
              << "  ping <ip>               : Send ping data frame to target IP\n"
              << "  arp-req <ip>            : Explicitly broadcast ARP Request for target IP\n"
              << "  gratuitous              : Broadcast Gratuitous ARP announcement\n"
              << "  static <ip> <mac>       : Add permanent static mapping to ARP cache\n"
              << "  poison <v_ip> <v_mac> <fake_ip> <fake_mac> : Send malicious spoofed ARP reply\n"
              << "  flush                   : Flush dynamic ARP cache entries\n"
              << "  help                    : Show this help menu\n"
              << "  exit | quit             : Exit host CLI\n"
              << "-----------------------------------\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <host_name> <ip_address> <mac_address> [switch_ip] [switch_port]\n"
                  << "Example: " << argv[0] << " HostA 192.168.1.10 00:11:22:33:44:0A 127.0.0.1 8000\n";
        return 1;
    }

    std::string name = argv[1];
    arp::IpAddress ip(argv[2]);
    arp::MacAddress mac(argv[3]);
    std::string switch_ip = (argc >= 5) ? argv[4] : "127.0.0.1";
    uint16_t switch_port = (argc >= 6) ? static_cast<uint16_t>(std::stoi(argv[5])) : 8000;

    // Default interactive TTL: 300 seconds (5 minutes) to prevent premature cache expiration during manual testing
    arp::HostNode host(name, mac, ip, std::chrono::seconds(300));
    if (!host.start(switch_ip, switch_port)) {
        return 1;
    }

    print_help();

    std::string line;
    while (host.is_running()) {
        std::cout << arp::colors::BOLD << name << " (" << ip << ")> " << arp::colors::RESET;
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) {
            continue;
        }

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "show" || cmd == "arp" || cmd == "arp-a" || cmd == "table") {
            std::string sub;
            iss >> sub;
            host.print_arp_table();
        } else if (cmd == "send") {
            std::string target_ip_str, msg;
            if (iss >> target_ip_str) {
                std::getline(iss, msg);
                // trim leading space
                if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
                if (msg.empty()) msg = "Hello from " + name;
                host.send_data(arp::IpAddress(target_ip_str), msg);
            } else {
                std::cout << "Usage: send <target_ip> <message>\n";
            }
        } else if (cmd == "ping") {
            std::string target_ip_str;
            if (iss >> target_ip_str) {
                host.send_data(arp::IpAddress(target_ip_str), "PING_ECHO_REQUEST");
            } else {
                std::cout << "Usage: ping <target_ip>\n";
            }
        } else if (cmd == "arp-req") {
            std::string target_ip_str;
            if (iss >> target_ip_str) {
                host.send_arp_request(arp::IpAddress(target_ip_str));
            } else {
                std::cout << "Usage: arp-req <target_ip>\n";
            }
        } else if (cmd == "gratuitous") {
            host.send_gratuitous_arp();
        } else if (cmd == "static") {
            std::string sip, smac;
            if (iss >> sip >> smac) {
                host.get_cache().set_static(arp::IpAddress(sip), arp::MacAddress(smac));
                std::cout << "Added static ARP mapping: " << sip << " -> " << smac << "\n";
            } else {
                std::cout << "Usage: static <ip> <mac>\n";
            }
        } else if (cmd == "poison") {
            std::string vip, vmac, fip, fmac;
            if (iss >> vip >> vmac >> fip >> fmac) {
                host.send_spoofed_arp_reply(arp::IpAddress(vip), arp::MacAddress(vmac),
                                           arp::IpAddress(fip), arp::MacAddress(fmac));
            } else {
                std::cout << "Usage: poison <victim_ip> <victim_mac> <spoofed_ip> <spoofed_mac>\n";
            }
        } else if (cmd == "flush") {
            host.get_cache().flush_dynamic();
            std::cout << "Dynamic ARP cache flushed.\n";
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for command list.\n";
        }
    }

    host.stop();
    return 0;
}
