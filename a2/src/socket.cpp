#include "socket.hpp"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sstream>
#include <iostream>

using namespace std;

namespace net {

Endpoint::Endpoint() : port(0) {
    memset(&addr, 0, sizeof(addr));
}

Endpoint::Endpoint(const string& ip_str, uint16_t port_num)
    : ip(ip_str), port(port_num) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
}

Endpoint::Endpoint(const struct sockaddr_in& sa) {
    addr = sa;
    char ip_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_buf, INET_ADDRSTRLEN);
    ip = ip_buf;
    port = ntohs(addr.sin_port);
}

string Endpoint::to_string() const {
    ostringstream oss;
    oss << ip << ":" << port;
    return oss.str();
}

UdpSocket::UdpSocket() : fd_(-1), bound_(false) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        perror("Failed to create UDP socket");
    }
}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::bind(uint16_t port, const string& ip) {
    if (fd_ < 0) return false;

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);

    if (ip == "0.0.0.0" || ip.empty()) {
        bind_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip.c_str(), &bind_addr.sin_addr);
    }

    if (::bind(fd_, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("Failed to bind UDP socket");
        return false;
    }

    bound_ = true;
    return true;
}

uint16_t UdpSocket::get_bound_port() const {
    if (fd_ < 0) return 0;
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(fd_, (struct sockaddr*)&sin, &len) == 0) {
        return ntohs(sin.sin_port);
    }
    return 0;
}

ssize_t UdpSocket::send_to(const uint8_t* data, size_t len, const Endpoint& dest) {
    if (fd_ < 0) return -1;
    return ::sendto(fd_, data, len, 0,
                    (const struct sockaddr*)&dest.addr, sizeof(dest.addr));
}

ssize_t UdpSocket::send_to(const vector<uint8_t>& data, const Endpoint& dest) {
    return send_to(data.data(), data.size(), dest);
}

ssize_t UdpSocket::recv_from(uint8_t* buffer, size_t max_len, Endpoint& src, int timeout_ms) {
    if (fd_ < 0) return -1;

    if (timeout_ms >= 0) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = ::poll(&pfd, 1, timeout_ms);
        if (ret == 0) {
            return 0; // timeout
        } else if (ret < 0) {
            return -1; // error
        }
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    ssize_t bytes = ::recvfrom(fd_, buffer, max_len, 0,
                               (struct sockaddr*)&sender_addr, &addr_len);

    if (bytes > 0) {
        src = Endpoint(sender_addr);
    }
    return bytes;
}

ssize_t UdpSocket::recv_from(vector<uint8_t>& buffer, Endpoint& src, int timeout_ms) {
    if (buffer.size() < 2048) {
        buffer.resize(2048);
    }
    ssize_t bytes = recv_from(buffer.data(), buffer.size(), src, timeout_ms);
    if (bytes > 0) {
        buffer.resize(bytes);
    } else {
        buffer.clear();
    }
    return bytes;
}

void UdpSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    bound_ = false;
}

} // namespace net
