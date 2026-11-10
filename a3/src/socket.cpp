#include "socket.hpp"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sstream>
#include <chrono>
#include <thread>
#include <iostream>

namespace csma {

Endpoint::Endpoint() : port(0), is_unix(true) {
    std::memset(&un_addr, 0, sizeof(un_addr));
    std::memset(&in_addr, 0, sizeof(in_addr));
    un_addr.sun_family = AF_UNIX;
    in_addr.sin_family = AF_INET;
#ifdef __APPLE__
    un_addr.sun_len = sizeof(struct sockaddr_un);
    in_addr.sin_len = sizeof(struct sockaddr_in);
#endif
}

Endpoint::Endpoint(const std::string& unix_path) : path(unix_path), port(0), is_unix(true) {
    std::memset(&un_addr, 0, sizeof(un_addr));
    std::memset(&in_addr, 0, sizeof(in_addr));
    un_addr.sun_family = AF_UNIX;
    std::strncpy(un_addr.sun_path, unix_path.c_str(), sizeof(un_addr.sun_path) - 1);
#ifdef __APPLE__
    un_addr.sun_len = SUN_LEN(&un_addr);
#endif
}

Endpoint::Endpoint(const std::string& ip_str, uint16_t port_num)
    : path(""), ip(ip_str), port(port_num), is_unix(false) {
    std::memset(&un_addr, 0, sizeof(un_addr));
    std::memset(&in_addr, 0, sizeof(in_addr));
    in_addr.sin_family = AF_INET;
#ifdef __APPLE__
    in_addr.sin_len = sizeof(struct sockaddr_in);
#endif
    in_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &in_addr.sin_addr);
}

Endpoint::Endpoint(const struct sockaddr_un& sa) : is_unix(true) {
    un_addr = sa;
    std::memset(&in_addr, 0, sizeof(in_addr));
    path = std::string(un_addr.sun_path);
    port = 0;
#ifdef __APPLE__
    un_addr.sun_len = SUN_LEN(&un_addr);
#endif
}

Endpoint::Endpoint(const struct sockaddr_in& sa) : is_unix(false) {
    in_addr = sa;
    std::memset(&un_addr, 0, sizeof(un_addr));
    char ip_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(in_addr.sin_addr), ip_buf, INET_ADDRSTRLEN);
    ip = ip_buf;
    port = ntohs(in_addr.sin_port);
}

std::string Endpoint::to_string() const {
    if (is_unix) {
        return "unix:" + path;
    }
    std::ostringstream oss;
    oss << ip << ":" << port;
    return oss.str();
}

UdpSocket::UdpSocket() : fd_(-1), bound_(false), is_unix_(true) {}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open_unix() {
    if (fd_ >= 0) {
        if (is_unix_) return true;
        close();
    }
    fd_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;
    is_unix_ = true;
    return true;
}

bool UdpSocket::open_inet() {
    if (fd_ >= 0) {
        if (!is_unix_) return true;
        close();
    }
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;
    is_unix_ = false;

    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    return true;
}

void UdpSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (bound_ && is_unix_ && !bound_path_.empty()) {
        ::unlink(bound_path_.c_str());
        bound_path_.clear();
    }
    bound_ = false;
}

bool UdpSocket::bind_unix(const std::string& path) {
    if (!open_unix()) return false;

    ::unlink(path.c_str());

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
#ifdef __APPLE__
    addr.sun_len = SUN_LEN(&addr);
    socklen_t len = addr.sun_len;
#else
    socklen_t len = sizeof(addr);
#endif

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), len) < 0) {
        perror("[bind_unix error]");
        return false;
    }

    bound_ = true;
    bound_path_ = path;
    return true;
}

bool UdpSocket::bind_inet(uint16_t port, const std::string& ip) {
    if (!open_inet()) return false;

    struct sockaddr_in bind_addr;
    std::memset(&bind_addr, 0, sizeof(bind_addr));
#ifdef __APPLE__
    bind_addr.sin_len = sizeof(struct sockaddr_in);
#endif
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);

    if (ip == "0.0.0.0" || ip.empty()) {
        bind_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip.c_str(), &bind_addr.sin_addr);
    }

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        return false;
    }

    bound_ = true;
    return true;
}

bool UdpSocket::bind_auto(uint16_t port, uint32_t station_id) {
    std::string path;
    if (station_id == 0) {
        path = "/tmp/csma_channel_" + std::to_string(port) + ".sock";
    } else {
        path = "/tmp/csma_station_" + std::to_string(station_id) + "_" + std::to_string(getpid()) + ".sock";
    }
    return bind_unix(path);
}

ssize_t UdpSocket::send_to(const uint8_t* data, size_t len, const Endpoint& dest) {
    if (fd_ < 0) return -1;
    if (dest.is_unix) {
#ifdef __APPLE__
        socklen_t addrlen = dest.un_addr.sun_len > 0 ? dest.un_addr.sun_len : SUN_LEN(&dest.un_addr);
#else
        socklen_t addrlen = sizeof(dest.un_addr);
#endif
        ssize_t ret = ::sendto(fd_, data, len, 0,
                               reinterpret_cast<const struct sockaddr*>(&dest.un_addr), addrlen);
        if (ret < 0) {
            perror("[UdpSocket::send_to unix error]");
        }
        return ret;
    } else {
        return ::sendto(fd_, data, len, 0,
                        reinterpret_cast<const struct sockaddr*>(&dest.in_addr), sizeof(dest.in_addr));
    }
}

ssize_t UdpSocket::send_to(const std::vector<uint8_t>& data, const Endpoint& dest) {
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
        if (ret <= 0) {
            return 0; // timeout or signal
        }
    }

    if (is_unix_) {
        struct sockaddr_un sender_un;
        socklen_t addr_len = sizeof(sender_un);
        std::memset(&sender_un, 0, sizeof(sender_un));
        ssize_t bytes = ::recvfrom(fd_, buffer, max_len, 0,
                                   reinterpret_cast<struct sockaddr*>(&sender_un), &addr_len);
        if (bytes > 0) {
            src = Endpoint(sender_un);
        }
        return bytes;
    } else {
        struct sockaddr_in sender_in;
        socklen_t addr_len = sizeof(sender_in);
        std::memset(&sender_in, 0, sizeof(sender_in));
        ssize_t bytes = ::recvfrom(fd_, buffer, max_len, 0,
                                   reinterpret_cast<struct sockaddr*>(&sender_in), &addr_len);
        if (bytes > 0) {
            src = Endpoint(sender_in);
        }
        return bytes;
    }
}

ssize_t UdpSocket::recv_from(std::vector<uint8_t>& buffer, Endpoint& src, int timeout_ms) {
    if (buffer.size() < 4096) {
        buffer.resize(4096);
    }
    ssize_t bytes = recv_from(buffer.data(), buffer.size(), src, timeout_ms);
    if (bytes > 0) {
        buffer.resize(bytes);
    } else {
        buffer.clear();
    }
    return bytes;
}

uint64_t get_current_time_us() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

uint64_t get_current_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void precise_sleep_ms(double ms) {
    if (ms <= 0.0) return;
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(ms));
}

void precise_sleep_us(uint64_t us) {
    if (us == 0) return;
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

} // namespace csma
