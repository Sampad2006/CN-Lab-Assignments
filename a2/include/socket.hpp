#ifndef A2_SOCKET_HPP
#define A2_SOCKET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>

namespace net {

using std::string;
using std::vector;

struct Endpoint {
    string ip;
    uint16_t port;
    struct sockaddr_in addr;

    Endpoint();
    Endpoint(const string& ip, uint16_t port);
    Endpoint(const struct sockaddr_in& sa);
    string to_string() const;
};

// posix udp socket wrapper
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    bool bind(uint16_t port, const string& ip = "0.0.0.0");
    ssize_t send_to(const uint8_t* data, size_t len, const Endpoint& dest);
    ssize_t send_to(const vector<uint8_t>& data, const Endpoint& dest);
    ssize_t recv_from(uint8_t* buffer, size_t max_len, Endpoint& src, int timeout_ms = -1);
    ssize_t recv_from(vector<uint8_t>& buffer, Endpoint& src, int timeout_ms = -1);

    int get_fd() const { return fd_; }
    uint16_t get_bound_port() const;
    void close();

private:
    int fd_;
    bool bound_;
};

} // namespace net

#endif // A2_SOCKET_HPP
