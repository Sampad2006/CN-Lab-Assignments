#include "types.hpp"
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace arp {

MacAddress::MacAddress() {
    bytes.fill(0);
}

MacAddress::MacAddress(const std::array<uint8_t, 6>& b) : bytes(b) {}

MacAddress::MacAddress(const uint8_t* raw) {
    if (raw) {
        std::memcpy(bytes.data(), raw, 6);
    } else {
        bytes.fill(0);
    }
}

MacAddress::MacAddress(const std::string& str) {
    unsigned int b[6];
    if (std::sscanf(str.c_str(), "%x:%x:%x:%x:%x:%x",
                    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6 ||
        std::sscanf(str.c_str(), "%x-%x-%x-%x-%x-%x",
                    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (int i = 0; i < 6; ++i) {
            bytes[i] = static_cast<uint8_t>(b[i]);
        }
    } else {
        bytes.fill(0);
    }
}

std::string MacAddress::to_string() const {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return std::string(buf);
}

bool MacAddress::is_broadcast() const {
    return *this == broadcast();
}

bool MacAddress::is_zero() const {
    return *this == zero();
}

bool MacAddress::operator==(const MacAddress& other) const {
    return bytes == other.bytes;
}

bool MacAddress::operator!=(const MacAddress& other) const {
    return !(*this == other);
}

bool MacAddress::operator<(const MacAddress& other) const {
    return bytes < other.bytes;
}

MacAddress MacAddress::broadcast() {
    return MacAddress(std::array<uint8_t, 6>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
}

MacAddress MacAddress::zero() {
    return MacAddress();
}

// ---------------- IpAddress ----------------

IpAddress::IpAddress() : addr(0) {}

IpAddress::IpAddress(uint32_t host_order_ip) : addr(host_order_ip) {}

IpAddress::IpAddress(const std::string& str) {
    struct in_addr in;
    if (inet_aton(str.c_str(), &in)) {
        addr = ntohl(in.s_addr);
    } else {
        addr = 0;
    }
}

IpAddress::IpAddress(const uint8_t* raw_network_order) {
    if (raw_network_order) {
        uint32_t net;
        std::memcpy(&net, raw_network_order, 4);
        addr = ntohl(net);
    } else {
        addr = 0;
    }
}

std::string IpAddress::to_string() const {
    struct in_addr in;
    in.s_addr = htonl(addr);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &in, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "0.0.0.0";
}

uint32_t IpAddress::to_network_order() const {
    return htonl(addr);
}

uint32_t IpAddress::to_host_order() const {
    return addr;
}

bool IpAddress::is_zero() const {
    return addr == 0;
}

bool IpAddress::is_broadcast() const {
    return addr == 0xFFFFFFFF;
}

bool IpAddress::operator==(const IpAddress& other) const {
    return addr == other.addr;
}

bool IpAddress::operator!=(const IpAddress& other) const {
    return addr != other.addr;
}

bool IpAddress::operator<(const IpAddress& other) const {
    return addr < other.addr;
}

IpAddress IpAddress::any() {
    return IpAddress(static_cast<uint32_t>(0));
}

IpAddress IpAddress::broadcast() {
    return IpAddress(static_cast<uint32_t>(0xFFFFFFFF));
}

std::ostream& operator<<(std::ostream& os, const MacAddress& mac) {
    return os << mac.to_string();
}

std::ostream& operator<<(std::ostream& os, const IpAddress& ip) {
    return os << ip.to_string();
}

} // namespace arp
