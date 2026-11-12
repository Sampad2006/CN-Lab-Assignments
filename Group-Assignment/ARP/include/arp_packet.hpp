#ifndef ARP_PACKET_HPP
#define ARP_PACKET_HPP

#include "types.hpp"
#include "ethernet.hpp"
#include <string>
#include <vector>

namespace arp {

enum class ArpOpcode : uint16_t {
    REQUEST = 1,
    REPLY   = 2,
    RARP_REQUEST = 3,
    RARP_REPLY   = 4
};

#pragma pack(push, 1)
struct ArpRawHeader {
    uint16_t htype;         // Hardware Type (1 for Ethernet)
    uint16_t ptype;         // Protocol Type (0x0800 for IPv4)
    uint8_t  hlen;          // Hardware Address Length (6)
    uint8_t  plen;          // Protocol Address Length (4)
    uint16_t opcode;        // 1=Request, 2=Reply
    uint8_t  sender_mac[6]; // SHA
    uint32_t sender_ip;     // SPA (network byte order)
    uint8_t  target_mac[6]; // THA
    uint32_t target_ip;     // TPA (network byte order)
};
#pragma pack(pop)

static_assert(sizeof(ArpRawHeader) == 28, "ArpRawHeader must be exactly 28 bytes per RFC 826");

class ArpPacket {
public:
    uint16_t htype{1};             // Ethernet
    uint16_t ptype{ETHERTYPE_IPV4};// IPv4
    uint8_t  hlen{6};              // 6 bytes MAC
    uint8_t  plen{4};              // 4 bytes IP
    ArpOpcode opcode{ArpOpcode::REQUEST};

    MacAddress sender_mac;
    IpAddress  sender_ip;
    MacAddress target_mac;
    IpAddress  target_ip;

    ArpPacket() = default;

    static ArpPacket make_request(const MacAddress& sender_mac,
                                  const IpAddress& sender_ip,
                                  const IpAddress& target_ip);

    static ArpPacket make_reply(const MacAddress& sender_mac,
                                const IpAddress& sender_ip,
                                const MacAddress& target_mac,
                                const IpAddress& target_ip);

    static ArpPacket make_gratuitous(const MacAddress& sender_mac,
                                     const IpAddress& ip);

    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* data, size_t len, ArpPacket& out_pkt);

    bool is_gratuitous() const;
    std::string to_string() const;
    std::string disassemble_summary() const;
};

} // namespace arp

#endif // ARP_PACKET_HPP
