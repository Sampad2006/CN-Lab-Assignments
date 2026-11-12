#ifndef ARP_VIRTUAL_SWITCH_HPP
#define ARP_VIRTUAL_SWITCH_HPP

#include "types.hpp"
#include "ethernet.hpp"
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <netinet/in.h>

namespace arp {

struct SwitchPortEntry {
    MacAddress mac;
    struct sockaddr_in client_addr{};
    std::chrono::steady_clock::time_point last_seen;
};

class VirtualSwitch {
private:
    int server_sockfd_{-1};
    uint16_t listen_port_{8000};
    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    mutable std::mutex cam_mutex_;
    // CAM table: MAC -> SwitchPortEntry
    std::unordered_map<MacAddress, SwitchPortEntry> cam_table_;
    // Active client addresses (endpoints)
    std::vector<struct sockaddr_in> active_clients_;

    void receive_loop();
    void forward_frame(const uint8_t* raw_frame, size_t len, const struct sockaddr_in& sender_addr);
    bool is_same_endpoint(const struct sockaddr_in& a, const struct sockaddr_in& b) const;

public:
    explicit VirtualSwitch(uint16_t listen_port = 8000);
    ~VirtualSwitch();

    bool start();
    void stop();
    bool is_running() const { return running_; }

    uint16_t get_port() const { return listen_port_; }

    // Print CAM table
    std::string format_cam_table() const;
    void print_cam_table(std::ostream& os) const;

    size_t active_ports_count() const;
};

} // namespace arp

#endif // ARP_VIRTUAL_SWITCH_HPP
