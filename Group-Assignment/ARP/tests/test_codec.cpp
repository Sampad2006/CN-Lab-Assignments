#include "arp_packet.hpp"
#include "ethernet.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

void test_mac_and_ip() {
    arp::MacAddress mac("00:1A:2B:3C:4D:5E");
    assert(mac.to_string() == "00:1A:2B:3C:4D:5E");
    assert(!mac.is_broadcast());
    assert(!mac.is_zero());

    arp::MacAddress bcast = arp::MacAddress::broadcast();
    assert(bcast.is_broadcast());
    assert(bcast.to_string() == "FF:FF:FF:FF:FF:FF");

    arp::IpAddress ip("192.168.1.1");
    assert(ip.to_string() == "192.168.1.1");
    assert(ip.to_host_order() == 0xC0A80101);

    std::cout << "[PASS] MacAddress & IpAddress tests passed.\n";
}

void test_arp_packet_codec() {
    arp::MacAddress src_mac("00:11:22:33:44:55");
    arp::IpAddress src_ip("10.0.0.1");
    arp::IpAddress target_ip("10.0.0.2");

    arp::ArpPacket req = arp::ArpPacket::make_request(src_mac, src_ip, target_ip);
    assert(req.opcode == arp::ArpOpcode::REQUEST);
    assert(req.htype == 1);
    assert(req.ptype == 0x0800);
    assert(req.hlen == 6);
    assert(req.plen == 4);
    assert(req.sender_mac == src_mac);
    assert(req.sender_ip == src_ip);
    assert(req.target_mac.is_zero());
    assert(req.target_ip == target_ip);

    auto bytes = req.serialize();
    assert(bytes.size() == 28); // RFC 826 exactly 28 bytes!

    arp::ArpPacket decoded;
    bool ok = arp::ArpPacket::deserialize(bytes.data(), bytes.size(), decoded);
    assert(ok);
    assert(decoded.opcode == arp::ArpOpcode::REQUEST);
    assert(decoded.sender_mac == src_mac);
    assert(decoded.sender_ip == src_ip);
    assert(decoded.target_mac == arp::MacAddress::zero());
    assert(decoded.target_ip == target_ip);

    std::cout << "[PASS] RFC 826 ARP 28-byte codec test passed.\n";
}

void test_ethernet_frame_codec() {
    arp::MacAddress dst("AA:BB:CC:DD:EE:FF");
    arp::MacAddress src("11:22:33:44:55:66");

    arp::EthernetFrame frame;
    frame.dst = dst;
    frame.src = src;
    frame.ethertype = arp::ETHERTYPE_ARP;
    frame.payload = {0x01, 0x02, 0x03, 0x04};

    auto raw = frame.serialize();
    assert(raw.size() == 14 + 4);

    arp::EthernetFrame decoded;
    bool ok = arp::EthernetFrame::deserialize(raw.data(), raw.size(), decoded);
    assert(ok);
    assert(decoded.dst == dst);
    assert(decoded.src == src);
    assert(decoded.ethertype == arp::ETHERTYPE_ARP);
    assert(decoded.payload.size() == 4);

    std::cout << "[PASS] Ethernet frame 14-byte header codec test passed.\n";
}

int main() {
    std::cout << "Running Codec & Serialization Unit Tests...\n";
    test_mac_and_ip();
    test_arp_packet_codec();
    test_ethernet_frame_codec();
    std::cout << "All Codec Tests PASSED!\n";
    return 0;
}
