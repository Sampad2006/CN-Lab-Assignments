#include "socket_utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/time.h>
#include <cstring>
#include <cerrno>
#include <iostream>

namespace ftp {

int SocketUtils::create_tcp_listener(const std::string& bind_ip, uint16_t port, int backlog) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    set_reuse_addr(listen_fd);

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (bind_ip.empty() || bind_ip == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (::inet_pton(AF_INET, bind_ip.c_str(), &addr.sin_addr) <= 0) {
            ::close(listen_fd);
            return -1;
        }
    }

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd);
        return -1;
    }

    if (::listen(listen_fd, backlog) < 0) {
        ::close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int SocketUtils::connect_tcp(const std::string& host_ip, uint16_t port, int timeout_sec) {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host_ip.c_str(), &addr.sin_addr) <= 0) {
        // Fallback to DNS resolve
        struct hostent* he = ::gethostbyname(host_ip.c_str());
        if (!he) {
            ::close(sockfd);
            return -1;
        }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    set_socket_timeout(sockfd, timeout_sec > 0 ? timeout_sec : CONTROL_TIMEOUT_SEC);

#ifdef SO_NOSIGPIPE
    int nosigpipe = 1;
    ::setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif

    if (::connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sockfd);
        return -1;
    }

    return sockfd;
}

int SocketUtils::accept_client(int listen_fd, std::string& client_ip, uint16_t& client_port) {
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    std::memset(&client_addr, 0, sizeof(client_addr));

    int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) return -1;

    char ip_buf[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf))) {
        client_ip = ip_buf;
    } else {
        client_ip = "127.0.0.1";
    }
    client_port = ntohs(client_addr.sin_port);

    set_socket_timeout(client_fd, CONTROL_TIMEOUT_SEC);

#ifdef SO_NOSIGPIPE
    int nosigpipe = 1;
    ::setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
    return client_fd;
}

bool SocketUtils::get_socket_address(int sockfd, std::string& ip_out, uint16_t& port_out) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) return false;

    char ip_buf[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf))) {
        ip_out = ip_buf;
    } else {
        ip_out = "127.0.0.1";
    }
    port_out = ntohs(addr.sin_port);
    return true;
}

bool SocketUtils::get_peer_address(int sockfd, std::string& ip_out, uint16_t& port_out) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (::getpeername(sockfd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) return false;

    char ip_buf[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf))) {
        ip_out = ip_buf;
    } else {
        ip_out = "127.0.0.1";
    }
    port_out = ntohs(addr.sin_port);
    return true;
}

bool SocketUtils::set_socket_timeout(int sockfd, int sec, int usec) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    int r1 = ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int r2 = ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return (r1 == 0 && r2 == 0);
}

bool SocketUtils::set_reuse_addr(int sockfd) {
    int opt = 1;
    return (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0);
}

bool SocketUtils::set_blocking(int sockfd, bool blocking) {
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return ::fcntl(sockfd, F_SETFL, flags) == 0;
}

bool SocketUtils::send_all(int sockfd, const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;
    while (remaining > 0) {
#ifdef MSG_NOSIGNAL
        ssize_t sent = ::send(sockfd, ptr, remaining, MSG_NOSIGNAL);
#else
        ssize_t sent = ::send(sockfd, ptr, remaining, 0);
#endif
        if (sent <= 0) {
            if (sent < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool SocketUtils::send_line(int sockfd, const std::string& line) {
    std::string wire = line;
    if (wire.size() < 2 || wire.substr(wire.size() - 2) != "\r\n") {
        wire += "\r\n";
    }
    return send_all(sockfd, wire.data(), wire.size());
}

bool SocketUtils::recv_line(int sockfd, std::string& line, size_t max_len) {
    line.clear();
    char ch;
    while (line.size() < max_len) {
        ssize_t r = ::recv(sockfd, &ch, 1, 0);
        if (r <= 0) {
            if (r < 0 && (errno == EINTR)) continue;
            return false; // Connection closed or timeout error
        }
        if (ch == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }
        line.push_back(ch);
    }
    return true;
}

ssize_t SocketUtils::recv_chunk(int sockfd, void* buffer, size_t max_bytes) {
    return ::recv(sockfd, buffer, max_bytes, 0);
}

void SocketUtils::close_socket(int& sockfd) {
    if (sockfd >= 0) {
        ::shutdown(sockfd, SHUT_RDWR);
        ::close(sockfd);
        sockfd = -1;
    }
}

} // namespace ftp
