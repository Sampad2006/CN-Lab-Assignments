#ifndef ARP_ETHERNET_HPP
#define ARP_ETHERNET_HPP

#include "types.hpp"
#include <vector>

namespace arp {

#pragma pack(push, 1)
struct EthernetHeader {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype; // Big-endian
};
#pragma pack(pop)

constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
constexpr uint16_t ETHERTYPE_ARP  = 0x0806;
constexpr uint16_t ETHERTYPE_DATA = 0x88B5; // IEEE 802 Local Experimental

struct EthernetFrame {
    MacAddress dst;
    MacAddress src;
    uint16_t ethertype{ETHERTYPE_ARP};
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* data, size_t len, EthernetFrame& out_frame);
};

} // namespace arp

#endif // ARP_ETHERNET_HPP
