#ifndef ARP_TYPES_HPP
#define ARP_TYPES_HPP

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <functional>
#include <ostream>
#include <arpa/inet.h>

namespace arp {

// 48-bit MAC Hardware Address
struct MacAddress {
    std::array<uint8_t, 6> bytes{};

    MacAddress();
    explicit MacAddress(const std::array<uint8_t, 6>& b);
    explicit MacAddress(const uint8_t* raw);
    explicit MacAddress(const std::string& str);

    std::string to_string() const;
    bool is_broadcast() const;
    bool is_zero() const;

    bool operator==(const MacAddress& other) const;
    bool operator!=(const MacAddress& other) const;
    bool operator<(const MacAddress& other) const;

    static MacAddress broadcast();
    static MacAddress zero();
};

// 32-bit IPv4 Protocol Address
struct IpAddress {
    uint32_t addr{0}; // In host byte order

    IpAddress();
    explicit IpAddress(uint32_t host_order_ip);
    explicit IpAddress(const std::string& str);
    explicit IpAddress(const uint8_t* raw_network_order);

    std::string to_string() const;
    uint32_t to_network_order() const;
    uint32_t to_host_order() const;
    bool is_zero() const;
    bool is_broadcast() const;

    bool operator==(const IpAddress& other) const;
    bool operator!=(const IpAddress& other) const;
    bool operator<(const IpAddress& other) const;

    static IpAddress any();
    static IpAddress broadcast();
};

std::ostream& operator<<(std::ostream& os, const MacAddress& mac);
std::ostream& operator<<(std::ostream& os, const IpAddress& ip);

} // namespace arp

// Hash specializations for std::unordered_map
namespace std {
    template <>
    struct hash<arp::MacAddress> {
        size_t operator()(const arp::MacAddress& mac) const noexcept {
            size_t h = 0;
            for (uint8_t b : mac.bytes) {
                h = (h * 31) ^ b;
            }
            return h;
        }
    };

    template <>
    struct hash<arp::IpAddress> {
        size_t operator()(const arp::IpAddress& ip) const noexcept {
            return std::hash<uint32_t>()(ip.addr);
        }
    };
}

#endif // ARP_TYPES_HPP
