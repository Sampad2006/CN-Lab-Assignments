#ifndef A4_SOCKET_HPP
#define A4_SOCKET_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "frame.hpp"

namespace cdma {

class TcpSocket {
public:
    TcpSocket();
    explicit TcpSocket(int fd);
    ~TcpSocket();

    // Non-copyable, movable
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    bool connect_to(const std::string& host, uint16_t port);
    void close();

    bool is_open() const { return fd_ >= 0; }
    int get_fd() const { return fd_; }

    bool set_timeout(int timeout_seconds);
    bool set_nodelay(bool enable = true);

    // Reliable stream transmission
    bool send_all(const uint8_t* data, size_t len);
    bool send_all(const std::vector<uint8_t>& data);

    bool recv_exact(uint8_t* buf, size_t len);
    bool recv_exact(std::vector<uint8_t>& buf, size_t len);

    // High-level packet I/O with automatic framing
    bool send_packet(const Packet& pkt);
    bool recv_packet(Packet& out_pkt);

private:
    int fd_;
};

class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool listen_on(uint16_t port, const std::string& bind_ip = "0.0.0.0", int backlog = 64);
    std::unique_ptr<TcpSocket> accept_client(std::string& out_ip, uint16_t& out_port);
    void close();

    bool is_listening() const { return listen_fd_ >= 0; }
    uint16_t get_port() const { return bound_port_; }

private:
    int listen_fd_;
    uint16_t bound_port_;
};

uint64_t get_current_time_us();
uint64_t get_current_time_ms();
void sleep_ms(double ms);

} // namespace cdma

#endif // A4_SOCKET_HPP
