#include "host_node.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace arp {

HostNode::HostNode(const std::string& name,
                   const MacAddress& mac,
                   const IpAddress& ip,
                   std::chrono::seconds cache_ttl)
    : name_(name), mac_(mac), ip_(ip), arp_cache_(cache_ttl) {}

HostNode::~HostNode() {
    stop();
}

bool HostNode::start(const std::string& switch_ip, uint16_t switch_port) {
    if (running_) {
        return true;
    }

    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        Logger::error(name_, "Failed to create host UDP socket");
        return false;
    }

    int opt = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200ms
    setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Bind to ephemeral port
    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(0);

    if (bind(sockfd_, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        Logger::error(name_, "Failed to bind host socket");
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    std::memset(&switch_addr_, 0, sizeof(switch_addr_));
    switch_addr_.sin_family = AF_INET;
    switch_addr_.sin_port = htons(switch_port);
    inet_pton(AF_INET, switch_ip.c_str(), &switch_addr_.sin_addr);

    running_ = true;
    rx_thread_ = std::thread(&HostNode::receive_loop, this);
    timer_thread_ = std::thread(&HostNode::timer_loop, this);

    // Register host presence with virtual switch by sending a registration frame
    send_gratuitous_arp();

    Logger::success(name_, "Host online [IP: " + ip_.to_string() + ", MAC: " + mac_.to_string() + "]");
    return true;
}

void HostNode::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (rx_thread_.joinable()) {
        rx_thread_.join();
    }
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
    Logger::info(name_, "Host stopped.");
}

bool HostNode::send_raw_frame(const EthernetFrame& frame) {
    if (sockfd_ < 0) {
        return false;
    }
    auto raw = frame.serialize();
    ssize_t sent = sendto(sockfd_, raw.data(), raw.size(), 0,
                          reinterpret_cast<const struct sockaddr*>(&switch_addr_),
                          sizeof(switch_addr_));
    return sent == static_cast<ssize_t>(raw.size());
}

bool HostNode::send_arp_request(const IpAddress& target_ip) {
    arp_cache_.mark_incomplete(target_ip);
    ArpPacket arp_pkt = ArpPacket::make_request(mac_, ip_, target_ip);

    EthernetFrame frame;
    frame.dst = MacAddress::broadcast();
    frame.src = mac_;
    frame.ethertype = ETHERTYPE_ARP;
    frame.payload = arp_pkt.serialize();

    Logger::packet("TX", name_ + " -> Broadcast: " + arp_pkt.disassemble_summary());
    return send_raw_frame(frame);
}

bool HostNode::send_gratuitous_arp() {
    ArpPacket garp = ArpPacket::make_gratuitous(mac_, ip_);

    EthernetFrame frame;
    frame.dst = MacAddress::broadcast();
    frame.src = mac_;
    frame.ethertype = ETHERTYPE_ARP;
    frame.payload = garp.serialize();

    Logger::packet("TX", name_ + " -> Broadcast (Gratuitous ARP): " + garp.disassemble_summary());
    return send_raw_frame(frame);
}

bool HostNode::send_spoofed_arp_reply(const IpAddress& victim_ip,
                                      const MacAddress& victim_mac,
                                      const IpAddress& spoofed_ip,
                                      const MacAddress& spoofed_mac) {
    // Poison victim's cache by claiming spoofed_ip is at spoofed_mac
    ArpPacket poison = ArpPacket::make_reply(spoofed_mac, spoofed_ip, victim_mac, victim_ip);

    EthernetFrame frame;
    frame.dst = victim_mac;
    frame.src = spoofed_mac;
    frame.ethertype = ETHERTYPE_ARP;
    frame.payload = poison.serialize();

    Logger::warn(name_, "[SPOOF] Sending malicious ARP reply to " + victim_ip.to_string()
                 + ": " + spoofed_ip.to_string() + " is at " + spoofed_mac.to_string());
    return send_raw_frame(frame);
}

bool HostNode::send_data(const IpAddress& target_ip, const std::string& message) {
    if (target_ip == ip_) {
        Logger::info(name_, "Loopback data sent to self: \"" + message + "\"");
        return true;
    }

    MacAddress target_mac;
    if (arp_cache_.lookup(target_ip, target_mac)) {
        // Cache HIT: Send immediately
        Logger::info(name_, "ARP Cache HIT for " + target_ip.to_string() + " -> " + target_mac.to_string());

        EthernetFrame frame;
        frame.dst = target_mac;
        frame.src = mac_;
        frame.ethertype = ETHERTYPE_DATA;
        frame.payload.assign(message.begin(), message.end());

        Logger::packet("TX", name_ + " -> " + target_mac.to_string()
                       + " [DATA] \"" + message + "\" (" + std::to_string(message.size()) + "B)");
        return send_raw_frame(frame);
    }

    // Cache MISS: Queue outbound payload and trigger ARP resolution
    Logger::warn(name_, "ARP Cache MISS for " + target_ip.to_string() + ". Queueing packet & resolving via ARP.");
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        QueuedPacket qp;
        qp.target_ip = target_ip;
        qp.ethertype = ETHERTYPE_DATA;
        qp.payload.assign(message.begin(), message.end());
        pending_queue_.push_back(std::move(qp));
    }

    return send_arp_request(target_ip);
}

void HostNode::flush_pending_for_ip(const IpAddress& ip, const MacAddress& mac) {
    std::vector<QueuedPacket> to_send;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        auto it = std::remove_if(pending_queue_.begin(), pending_queue_.end(),
            [&](const QueuedPacket& qp) {
                if (qp.target_ip == ip) {
                    to_send.push_back(qp);
                    return true;
                }
                return false;
            });
        pending_queue_.erase(it, pending_queue_.end());
    }

    for (const auto& qp : to_send) {
        EthernetFrame frame;
        frame.dst = mac;
        frame.src = mac_;
        frame.ethertype = qp.ethertype;
        frame.payload = qp.payload;

        std::string msg(qp.payload.begin(), qp.payload.end());
        Logger::packet("TX", name_ + " -> " + mac.to_string()
                       + " [QUEUED DATA RELEASED] \"" + msg + "\"");
        send_raw_frame(frame);
    }
}

void HostNode::receive_loop() {
    uint8_t buffer[2048];
    while (running_) {
        struct sockaddr_in from_addr{};
        socklen_t addr_len = sizeof(from_addr);

        ssize_t n = recvfrom(sockfd_, buffer, sizeof(buffer), 0,
                             reinterpret_cast<struct sockaddr*>(&from_addr), &addr_len);
        if (n < 0) {
            continue; // Timeout
        }

        handle_ethernet_frame(buffer, static_cast<size_t>(n));
    }
}

void HostNode::handle_ethernet_frame(const uint8_t* buf, size_t len) {
    EthernetFrame frame;
    if (!EthernetFrame::deserialize(buf, len, frame)) {
        return;
    }

    // Drop if frame is not for me and not broadcast
    if (frame.dst != mac_ && !frame.dst.is_broadcast()) {
        return;
    }

    // Ignore frames sent by myself
    if (frame.src == mac_) {
        return;
    }

    if (on_frame_received_) {
        on_frame_received_(name_, frame);
    }

    if (frame.ethertype == ETHERTYPE_ARP) {
        ArpPacket arp_pkt;
        if (ArpPacket::deserialize(frame.payload.data(), frame.payload.size(), arp_pkt)) {
            handle_arp_packet(frame, arp_pkt);
        }
    } else if (frame.ethertype == ETHERTYPE_DATA) {
        handle_data_packet(frame);
    }
}

void HostNode::handle_arp_packet(const EthernetFrame& frame, const ArpPacket& arp_pkt) {
    (void)frame;
    Logger::packet("RX", name_ + " <- " + arp_pkt.disassemble_summary());

    // Check for IP address conflict via Gratuitous ARP
    if (arp_pkt.is_gratuitous()) {
        if (arp_pkt.sender_ip == ip_ && arp_pkt.sender_mac != mac_) {
            Logger::error(name_, "CRITICAL: IP Address Conflict detected for " + ip_.to_string()
                          + "! Another host has MAC: " + arp_pkt.sender_mac.to_string());
            return;
        }
    }

    if (arp_pkt.opcode == ArpOpcode::REQUEST) {
        // RFC 826 Algorithm:
        // Merge flag = false
        // If (sender_ip already in my cache) -> update with sender_mac
        // If (target_ip == my_ip):
        //    Update/insert (sender_ip -> sender_mac)
        //    Send ARP reply
        if (arp_pkt.target_ip == ip_) {
            arp_cache_.update_or_insert(arp_pkt.sender_ip, arp_pkt.sender_mac);
            Logger::info(name_, "Learned/Updated ARP cache: " + arp_pkt.sender_ip.to_string()
                         + " -> " + arp_pkt.sender_mac.to_string());

            // Reply to sender
            ArpPacket reply = ArpPacket::make_reply(mac_, ip_, arp_pkt.sender_mac, arp_pkt.sender_ip);
            EthernetFrame reply_frame;
            reply_frame.dst = arp_pkt.sender_mac;
            reply_frame.src = mac_;
            reply_frame.ethertype = ETHERTYPE_ARP;
            reply_frame.payload = reply.serialize();

            Logger::packet("TX", name_ + " -> " + arp_pkt.sender_mac.to_string() + " (Unicast ARP Reply): "
                           + reply.disassemble_summary());
            send_raw_frame(reply_frame);
        } else if (arp_cache_.contains(arp_pkt.sender_ip)) {
            // Passive update if entry already present in cache
            arp_cache_.update_or_insert(arp_pkt.sender_ip, arp_pkt.sender_mac);
        }
    } else if (arp_pkt.opcode == ArpOpcode::REPLY) {
        // ARP Reply targeted at this host
        if (arp_pkt.target_ip == ip_ || arp_pkt.target_mac == mac_) {
            if (arp_cache_.update_or_insert(arp_pkt.sender_ip, arp_pkt.sender_mac)) {
                Logger::success(name_, "Resolved & Cached: " + arp_pkt.sender_ip.to_string()
                                + " -> " + arp_pkt.sender_mac.to_string());
                // Release any queued data waiting for this IP
                flush_pending_for_ip(arp_pkt.sender_ip, arp_pkt.sender_mac);
            } else {
                Logger::warn(name_, "Ignored dynamic ARP reply for " + arp_pkt.sender_ip.to_string()
                             + ": Entry is STATIC (protection active)!");
            }
        }
    }
}

void HostNode::handle_data_packet(const EthernetFrame& frame) {
    std::string msg(frame.payload.begin(), frame.payload.end());
    Logger::success(name_, "Delivered DATA from " + frame.src.to_string()
                    + ": \"" + msg + "\" (" + std::to_string(frame.payload.size()) + " bytes)");
}

void HostNode::timer_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running_) break;
        arp_cache_.clean_expired();
    }
}

std::string HostNode::show_arp_table() const {
    return arp_cache_.format_table(name_, mac_, ip_);
}

void HostNode::print_arp_table() const {
    std::cout << show_arp_table();
}

void HostNode::print_arp_table(std::ostream& os) const {
    os << show_arp_table();
}

} // namespace arp
