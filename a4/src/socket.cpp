#include "socket.hpp"
#include <cstring>
#include <chrono>
#include <thread>
#include <netinet/tcp.h>
#include <fcntl.h>

namespace cdma {

TcpSocket::TcpSocket() : fd_(-1) {}

TcpSocket::TcpSocket(int fd) : fd_(fd) {}

TcpSocket::~TcpSocket() {
    close();     
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void TcpSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool TcpSocket::connect_to(const std::string& host, uint16_t port) {
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        close();
        return false;
    }

    if (::connect(fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close();
        return false;
    }

    set_nodelay(true);
    return true;
}

bool TcpSocket::set_timeout(int timeout_seconds) {
    if (fd_ < 0) return false;
    struct timeval tv;
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return false;
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return false;
    return true;
}

bool TcpSocket::set_nodelay(bool enable) {
    if (fd_ < 0) return false;
    int opt = enable ? 1 : 0;
    return setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
}

bool TcpSocket::send_all(const uint8_t* data, size_t len) {
    if (fd_ < 0 || data == nullptr) return false;
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = ::send(fd_, data + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

bool TcpSocket::send_all(const std::vector<uint8_t>& data) {
    return send_all(data.data(), data.size());
}

bool TcpSocket::recv_exact(uint8_t* buf, size_t len) {
    if (fd_ < 0 || buf == nullptr) return false;
    size_t total_recvd = 0;
    while (total_recvd < len) {
        ssize_t recvd = ::recv(fd_, buf + total_recvd, len - total_recvd, 0);
        if (recvd <= 0) {
            return false;
        }
        total_recvd += recvd;
    }
    return true;
}

bool TcpSocket::recv_exact(std::vector<uint8_t>& buf, size_t len) {
    buf.resize(len);
    return recv_exact(buf.data(), len);
}

bool TcpSocket::send_packet(const Packet& pkt) {
    std::vector<uint8_t> bytes = FrameCodec::serialize_packet(pkt);
    return send_all(bytes);
}

bool TcpSocket::recv_packet(Packet& out_pkt) {
    uint8_t hdr_buf[sizeof(FrameHeader)];
    if (!recv_exact(hdr_buf, sizeof(FrameHeader))) {
        return false;
    }

    FrameHeader hdr;
    if (!FrameCodec::deserialize_header(hdr_buf, sizeof(FrameHeader), hdr)) {
        return false;
    }

    std::vector<uint8_t> payload;
    if (hdr.payload_len > 0) {
        if (!recv_exact(payload, hdr.payload_len)) {
            return false;
        }
    }

    out_pkt.type = static_cast<MsgType>(hdr.type);
    out_pkt.station_id = hdr.station_id;
    out_pkt.round_seq = hdr.round_seq;
    out_pkt.payload = std::move(payload);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TcpServer Implementation
// ─────────────────────────────────────────────────────────────────────────────

TcpServer::TcpServer() : listen_fd_(-1), bound_port_(0) {}

TcpServer::~TcpServer() {
    close();
}

void TcpServer::close() {
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

bool TcpServer::listen_on(uint16_t port, const std::string& bind_ip, int backlog) {
    close();
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) return false;

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (bind_ip == "0.0.0.0") {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (::inet_pton(AF_INET, bind_ip.c_str(), &serv_addr.sin_addr) <= 0) {
            close();
            return false;
        }
    }

    if (::bind(listen_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close();
        return false;
    }

    if (port == 0) {
        socklen_t len = sizeof(serv_addr);
        if (::getsockname(listen_fd_, (struct sockaddr*)&serv_addr, &len) == 0) {
            bound_port_ = ntohs(serv_addr.sin_port);
        }
    } else {
        bound_port_ = port;
    }

    if (::listen(listen_fd_, backlog) < 0) {
        close();
        return false;
    }

    return true;
}

std::unique_ptr<TcpSocket> TcpServer::accept_client(std::string& out_ip, uint16_t& out_port) {
    if (listen_fd_ < 0) return nullptr;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return nullptr;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    out_ip = ip_str;
    out_port = ntohs(client_addr.sin_port);

    auto sock = std::make_unique<TcpSocket>(client_fd);
    sock->set_nodelay(true);
    return sock;
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility Timing Functions
// ─────────────────────────────────────────────────────────────────────────────

uint64_t get_current_time_us() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

uint64_t get_current_time_ms() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

void sleep_ms(double ms) {
    if (ms <= 0.0) return;
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(ms));
}

} // namespace cdma
