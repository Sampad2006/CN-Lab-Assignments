#ifndef FTP_SOCKET_UTILS_HPP
#define FTP_SOCKET_UTILS_HPP

#include <string>
#include <cstdint>
#include <cstddef>
#include "common.hpp"

namespace ftp {

class SocketUtils {
public:
    // Create and bind a TCP listening socket
    // If port is 0, OS allocates an ephemeral port
    static int create_tcp_listener(const std::string& bind_ip, uint16_t port, int backlog = 16);

    // Connect to a remote TCP endpoint with timeout
    static int connect_tcp(const std::string& host_ip, uint16_t port, int timeout_sec = 10);

    // Accept an incoming client connection
    static int accept_client(int listen_fd, std::string& client_ip, uint16_t& client_port);

    // Retrieve the locally bound IP address and port of a socket
    static bool get_socket_address(int sockfd, std::string& ip_out, uint16_t& port_out);

    // Retrieve peer's remote IP address and port
    static bool get_peer_address(int sockfd, std::string& ip_out, uint16_t& port_out);

    // Configure socket timeout for send and recv
    static bool set_socket_timeout(int sockfd, int sec, int usec = 0);

    // Configure SO_REUSEADDR and SO_REUSEPORT
    static bool set_reuse_addr(int sockfd);

    // Set non-blocking or blocking mode
    static bool set_blocking(int sockfd, bool blocking);

    // Robust transmission of entire byte buffer
    static bool send_all(int sockfd, const void* data, size_t len);

    // Send an ASCII string terminated with standard CRLF (\r\n)
    static bool send_line(int sockfd, const std::string& line);

    // Receive a line terminated by CRLF or LF into string
    static bool recv_line(int sockfd, std::string& line, size_t max_len = 4096);

    // Receive raw data stream chunk
    static ssize_t recv_chunk(int sockfd, void* buffer, size_t max_bytes);

    // Close socket safely
    static void close_socket(int& sockfd);
};

// RAII Socket handle to prevent socket leaks
class ScopedSocket {
public:
    ScopedSocket() : fd_(-1) {}
    explicit ScopedSocket(int fd) : fd_(fd) {}
    ~ScopedSocket() { reset(); }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }
    bool is_valid() const { return fd_ >= 0; }

    int release() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }

    void reset(int new_fd = -1) {
        if (fd_ >= 0) {
            SocketUtils::close_socket(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_;
};

} // namespace ftp

#endif // FTP_SOCKET_UTILS_HPP
