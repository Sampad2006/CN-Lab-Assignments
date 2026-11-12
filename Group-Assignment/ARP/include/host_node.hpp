#ifndef ARP_HOST_NODE_HPP
#define ARP_HOST_NODE_HPP

#include "types.hpp"
#include "ethernet.hpp"
#include "arp_packet.hpp"
#include "arp_cache.hpp"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <functional>
#include <netinet/in.h>

namespace arp {

struct QueuedPacket {
    IpAddress target_ip;
    uint16_t ethertype;
    std::vector<uint8_t> payload;
};

class HostNode {
private:
    std::string name_;
    MacAddress  mac_;
    IpAddress   ip_;

    int sockfd_{-1};
    struct sockaddr_in switch_addr_{};
    std::atomic<bool> running_{false};
    std::thread rx_thread_;
    std::thread timer_thread_;

    ArpCache arp_cache_;
    std::mutex queue_mutex_;
    std::vector<QueuedPacket> pending_queue_;

    // Optional packet receive callback
    std::function<void(const std::string&, const EthernetFrame&)> on_frame_received_;

    void receive_loop();
    void timer_loop();
    void handle_ethernet_frame(const uint8_t* buf, size_t len);
    void handle_arp_packet(const EthernetFrame& frame, const ArpPacket& arp_pkt);
    void handle_data_packet(const EthernetFrame& frame);
    void flush_pending_for_ip(const IpAddress& ip, const MacAddress& mac);
    bool send_raw_frame(const EthernetFrame& frame);

public:
    HostNode(const std::string& name,
             const MacAddress& mac,
             const IpAddress& ip,
             std::chrono::seconds cache_ttl = std::chrono::seconds(60));
    ~HostNode();

    bool start(const std::string& switch_ip = "127.0.0.1", uint16_t switch_port = 8000);
    void stop();
    bool is_running() const { return running_; }

    const std::string& get_name() const { return name_; }
    const MacAddress& get_mac() const { return mac_; }
    const IpAddress& get_ip() const { return ip_; }
    ArpCache& get_cache() { return arp_cache_; }
    const ArpCache& get_cache() const { return arp_cache_; }

    // Core networking operations
    bool send_data(const IpAddress& target_ip, const std::string& message);
    bool send_arp_request(const IpAddress& target_ip);
    bool send_gratuitous_arp();
    bool send_spoofed_arp_reply(const IpAddress& victim_ip,
                                const MacAddress& victim_mac,
                                const IpAddress& spoofed_ip,
                                const MacAddress& spoofed_mac);

    // Show IP to MAC mappings available at this host
    std::string show_arp_table() const;
    void print_arp_table() const;
    void print_arp_table(std::ostream& os) const;

    void set_frame_callback(std::function<void(const std::string&, const EthernetFrame&)> cb) {
        on_frame_received_ = std::move(cb);
    }
};

} // namespace arp

#endif // ARP_HOST_NODE_HPP
