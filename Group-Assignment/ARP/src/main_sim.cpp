#include "virtual_switch.hpp"
#include "host_node.hpp"
#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>

void print_banner(const std::string& title) {
    std::cout << "\n" << arp::colors::BOLD << arp::colors::CYAN
              << "========================================================================\n"
              << "  " << title << "\n"
              << "========================================================================"
              << arp::colors::RESET << "\n\n";
}

void print_step(const std::string& step) {
    std::cout << "\n" << arp::colors::BOLD << arp::colors::YELLOW
              << ">>> " << step << arp::colors::RESET << "\n";
}

int main() {
    print_banner("SIMULATION: RFC 826 ARP Protocol Suite & Host Cache Demonstration");

    uint16_t switch_port = 8899; // Isolated port for simulator
    arp::VirtualSwitch sw(switch_port);
    if (!sw.start()) {
        arp::Logger::error("Main", "Unable to start simulation virtual switch");
        return 1;
    }

    // Initialize 3 Hosts with a short TTL (6 seconds) to demonstrate aging
    std::chrono::seconds demo_ttl(6);
    arp::HostNode hostA("HostA", arp::MacAddress("00:11:22:33:44:0A"), arp::IpAddress("192.168.1.10"), demo_ttl);
    arp::HostNode hostB("HostB", arp::MacAddress("00:11:22:33:44:0B"), arp::IpAddress("192.168.1.20"), demo_ttl);
    arp::HostNode hostC("HostC_Attacker", arp::MacAddress("00:11:22:33:44:CC"), arp::IpAddress("192.168.1.30"), demo_ttl);

    hostA.start("127.0.0.1", switch_port);
    hostB.start("127.0.0.1", switch_port);
    hostC.start("127.0.0.1", switch_port);

    // Allow gratuitous registrations to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // Clear dynamic caches so we start from clean cold state
    hostA.get_cache().clear();
    hostB.get_cache().clear();
    hostC.get_cache().clear();

    // -------------------------------------------------------------
    // DEMO 1: Initial Cold ARP Tables
    // -------------------------------------------------------------
    print_banner("DEMO 1: Initial State (Cold ARP Caches)");
    std::cout << hostA.show_arp_table();
    std::cout << hostB.show_arp_table();

    // -------------------------------------------------------------
    // DEMO 2: Cold ARP Resolution & UDP Data Transmission
    // -------------------------------------------------------------
    print_banner("DEMO 2: Host A Sends UDP Data to Host B (Cache Miss -> ARP Request/Reply)");
    print_step("Host A attempts to send UDP data to 192.168.1.20 (Host B). Cache is cold.");
    hostA.send_data(arp::IpAddress("192.168.1.20"), "Hello Host B, this is Host A!");

    // Wait for resolution and packet delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    print_step("Displaying updated ARP cache tables at both hosts post-resolution:");
    std::cout << hostA.show_arp_table();
    std::cout << hostB.show_arp_table();

    // -------------------------------------------------------------
    // DEMO 3: Cache Hit (Warm Cache)
    // -------------------------------------------------------------
    print_banner("DEMO 3: Subsequent Transmission with Warm Cache (Cache Hit)");
    print_step("Host A sends another UDP packet to 192.168.1.20. No ARP request should be needed.");
    hostA.send_data(arp::IpAddress("192.168.1.20"), "Second packet - direct fast path via cached MAC!");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // -------------------------------------------------------------
    // DEMO 4: Virtual Switch CAM Table
    // -------------------------------------------------------------
    print_banner("DEMO 4: Virtual L2 Switch CAM (MAC Address Table) Inspection");
    sw.print_cam_table(std::cout);

    // -------------------------------------------------------------
    // DEMO 5: Cache TTL Expiration & Dynamic Re-Resolution
    // -------------------------------------------------------------
    print_banner("DEMO 5: ARP Cache Aging & Expiration (TTL = 6 seconds)");
    print_step("Waiting for ARP cache entries to expire (sleep 7 seconds)...");
    for (int i = 6; i > 0; --i) {
        std::cout << "  Aging countdown: " << i << "s remaining...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    print_step("Host A ARP Cache state after TTL expiration:");
    std::cout << hostA.show_arp_table();

    print_step("Host A sends new data after expiry -> Re-triggers fresh ARP Request/Reply:");
    hostA.send_data(arp::IpAddress("192.168.1.20"), "Re-connecting after cache timeout!");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << hostA.show_arp_table();

    // -------------------------------------------------------------
    // DEMO 6: Gratuitous ARP & IP Conflict Detection
    // -------------------------------------------------------------
    print_banner("DEMO 6: Gratuitous ARP & IP Address Conflict Detection");
    print_step("A Rogue station attempts to claim Host A's IP (192.168.1.10) with MAC 00:DE:AD:BE:EF:01:");
    arp::HostNode rogueNode("RogueNode", arp::MacAddress("00:DE:AD:BE:EF:01"), arp::IpAddress("192.168.1.10"));
    rogueNode.start("127.0.0.1", switch_port);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    rogueNode.stop();

    // -------------------------------------------------------------
    // DEMO 7: ARP Spoofing / Poisoning & Static Defense
    // -------------------------------------------------------------
    print_banner("DEMO 7: ARP Spoofing (Cache Poisoning) & Static Mapping Defense");
    print_step("Attacker Host C sends forged ARP reply to Host A, claiming 192.168.1.20 is at Attacker MAC:");
    hostC.send_spoofed_arp_reply(arp::IpAddress("192.168.1.10"), arp::MacAddress("00:11:22:33:44:0A"),
                                 arp::IpAddress("192.168.1.20"), arp::MacAddress("00:11:22:33:44:CC"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    print_step("Host A's ARP Cache poisoned by malicious reply:");
    std::cout << hostA.show_arp_table();

    print_step("Defensive Measure: Configuring Static (Permanent) ARP entry for 192.168.1.20:");
    hostA.get_cache().set_static(arp::IpAddress("192.168.1.20"), arp::MacAddress("00:11:22:33:44:0B"));
    std::cout << hostA.show_arp_table();

    print_step("Attacker Host C tries to poison Host A again:");
    hostC.send_spoofed_arp_reply(arp::IpAddress("192.168.1.10"), arp::MacAddress("00:11:22:33:44:0A"),
                                 arp::IpAddress("192.168.1.20"), arp::MacAddress("00:11:22:33:44:CC"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    print_step("Host A's ARP Cache remained protected due to STATIC entry policy:");
    std::cout << hostA.show_arp_table();

    // Clean up
    print_banner("SIMULATION COMPLETE: All scenarios successfully executed!");
    hostA.stop();
    hostB.stop();
    hostC.stop();
    sw.stop();

    return 0;
}
