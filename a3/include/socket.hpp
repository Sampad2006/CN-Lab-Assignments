#ifndef A3_SOCKET_HPP
#define A3_SOCKET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace csma {

struct Endpoint {
    std::string path;         // For UNIX domain socket IPC
    std::string ip;           // For UDP IP
    uint16_t port;            // For UDP Port
    bool is_unix;
    struct sockaddr_un un_addr;
    struct sockaddr_in in_addr;

    Endpoint();
    explicit Endpoint(const std::string& unix_path);
    Endpoint(const std::string& ip_str, uint16_t port_num);
    explicit Endpoint(const struct sockaddr_un& sa);
    explicit Endpoint(const struct sockaddr_in& sa);
    std::string to_string() const;
};

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    bool open_unix();
    bool open_inet();
    void close();

    bool bind_unix(const std::string& path);
    bool bind_inet(uint16_t port = 0, const std::string& ip = "0.0.0.0");
    bool bind_auto(uint16_t port, uint32_t station_id = 0);

    ssize_t send_to(const uint8_t* data, size_t len, const Endpoint& dest);
    ssize_t send_to(const std::vector<uint8_t>& data, const Endpoint& dest);

    ssize_t recv_from(uint8_t* buffer, size_t max_len, Endpoint& src, int timeout_ms = -1);
    ssize_t recv_from(std::vector<uint8_t>& buffer, Endpoint& src, int timeout_ms = -1);

    bool is_open() const { return fd_ >= 0; }
    int get_fd() const { return fd_; }
    const std::string& get_bound_path() const { return bound_path_; }

private:
    int fd_;
    bool bound_;
    bool is_unix_;
    std::string bound_path_;
};

uint64_t get_current_time_us();
uint64_t get_current_time_ms();
void precise_sleep_ms(double ms);
void precise_sleep_us(uint64_t us);

} // namespace csma

#endif // A3_SOCKET_HPP
