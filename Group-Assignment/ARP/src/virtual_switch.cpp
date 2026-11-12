#include "virtual_switch.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace arp {

VirtualSwitch::VirtualSwitch(uint16_t listen_port) : listen_port_(listen_port) {}

VirtualSwitch::~VirtualSwitch() {
    stop();
}

bool VirtualSwitch::is_same_endpoint(const struct sockaddr_in& a, const struct sockaddr_in& b) const {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

bool VirtualSwitch::start() {
    if (running_) {
        return true;
    }

    server_sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sockfd_ < 0) {
        Logger::error("Switch", "Failed to create switch UDP socket");
        return false;
    }

    int opt = 1;
    setsockopt(server_sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(server_sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // Set receive timeout so loop can exit gracefully on stop
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200ms
    setsockopt(server_sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(listen_port_);

    if (bind(server_sockfd_, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        Logger::error("Switch", "Failed to bind switch to UDP port " + std::to_string(listen_port_));
        close(server_sockfd_);
        server_sockfd_ = -1;
        return false;
    }

    running_ = true;
    worker_thread_ = std::thread(&VirtualSwitch::receive_loop, this);
    Logger::success("Switch", "Virtual L2 Ethernet Switch online on UDP port " + std::to_string(listen_port_));
    return true;
}

void VirtualSwitch::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (server_sockfd_ >= 0) {
        close(server_sockfd_);
        server_sockfd_ = -1;
    }
    Logger::info("Switch", "Virtual L2 Switch stopped.");
}

void VirtualSwitch::receive_loop() {
    uint8_t buffer[2048];
    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        ssize_t n = recvfrom(server_sockfd_, buffer, sizeof(buffer), 0,
                             reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
        if (n < 0) {
            // Timeout or interrupt
            continue;
        }

        if (n < static_cast<ssize_t>(sizeof(EthernetHeader))) {
            continue; // Runt frame
        }

        forward_frame(buffer, static_cast<size_t>(n), client_addr);
    }
}

void VirtualSwitch::forward_frame(const uint8_t* raw_frame, size_t len, const struct sockaddr_in& sender_addr) {
    const EthernetHeader* hdr = reinterpret_cast<const EthernetHeader*>(raw_frame);
    MacAddress src_mac(hdr->src_mac);
    MacAddress dst_mac(hdr->dest_mac);

    std::lock_guard<std::mutex> lock(cam_mutex_);

    // 1. Keep track of active client endpoints
    bool known_client = false;
    for (const auto& ep : active_clients_) {
        if (is_same_endpoint(ep, sender_addr)) {
            known_client = true;
            break;
        }
    }
    if (!known_client) {
        active_clients_.push_back(sender_addr);
    }

    // 2. CAM Learning: Learn or refresh source MAC -> endpoint mapping
    auto now = std::chrono::steady_clock::now();
    SwitchPortEntry entry;
    entry.mac = src_mac;
    entry.client_addr = sender_addr;
    entry.last_seen = now;
    cam_table_[src_mac] = entry;

    // 3. Forwarding logic
    if (dst_mac.is_broadcast()) {
        // FLOOD: Send out all ports except the incoming port
        for (const auto& ep : active_clients_) {
            if (!is_same_endpoint(ep, sender_addr)) {
                sendto(server_sockfd_, raw_frame, len, 0,
                       reinterpret_cast<const struct sockaddr*>(&ep), sizeof(ep));
            }
        }
    } else {
        // UNICAST: Lookup destination in CAM table
        auto it = cam_table_.find(dst_mac);
        if (it != cam_table_.end()) {
            // Forward directly to destination endpoint
            sendto(server_sockfd_, raw_frame, len, 0,
                   reinterpret_cast<const struct sockaddr*>(&it->second.client_addr),
                   sizeof(it->second.client_addr));
        } else {
            // Unknown unicast: flood to all ports except incoming
            for (const auto& ep : active_clients_) {
                if (!is_same_endpoint(ep, sender_addr)) {
                    sendto(server_sockfd_, raw_frame, len, 0,
                           reinterpret_cast<const struct sockaddr*>(&ep), sizeof(ep));
                }
            }
        }
    }
}

std::string VirtualSwitch::format_cam_table() const {
    std::lock_guard<std::mutex> lock(cam_mutex_);
    std::ostringstream oss;
    oss << "========================================================================\n";
    oss << " Virtual L2 Switch - MAC Address / CAM Forwarding Table\n";
    oss << "------------------------------------------------------------------------\n";
    oss << std::left
        << std::setw(20) << "MAC Address"
        << std::setw(24) << "Endpoint (UDP Host:Port)"
        << std::setw(12) << "VLAN"
        << "Type\n";
    oss << "------------------------------------------------------------------------\n";

    if (cam_table_.empty()) {
        oss << " (No learned dynamic MAC addresses)\n";
    } else {
        for (const auto& kv : cam_table_) {
            char ip_buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &kv.second.client_addr.sin_addr, ip_buf, sizeof(ip_buf));
            uint16_t port = ntohs(kv.second.client_addr.sin_port);
            std::string ep_str = std::string(ip_buf) + ":" + std::to_string(port);

            oss << std::left
                << std::setw(20) << kv.first.to_string()
                << std::setw(24) << ep_str
                << std::setw(12) << "1 (Default)"
                << "DYNAMIC\n";
        }
    }
    oss << "========================================================================\n";
    return oss.str();
}

void VirtualSwitch::print_cam_table(std::ostream& os) const {
    os << format_cam_table();
}

size_t VirtualSwitch::active_ports_count() const {
    std::lock_guard<std::mutex> lock(cam_mutex_);
    return active_clients_.size();
}

} // namespace arp
