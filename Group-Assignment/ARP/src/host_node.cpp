#include "host_node.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

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
        if (qp.ethertype == ETHERTYPE_FILE) {
            std::string filepath(qp.payload.begin(), qp.payload.end());
            uint32_t transfer_id = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
            Logger::info(name_, "ARP resolved! Releasing queued file transfer: " + filepath);
            transmit_file_stream(mac, filepath, transfer_id);
        } else {
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
    } else if (frame.ethertype == ETHERTYPE_FILE) {
        handle_file_packet(frame);
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

bool HostNode::send_file(const IpAddress& target_ip, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        Logger::error(name_, "Cannot open file for transfer: " + filepath);
        return false;
    }
    file.close();

    MacAddress target_mac;
    if (arp_cache_.lookup(target_ip, target_mac)) {
        Logger::info(name_, "ARP Cache HIT for file transfer to " + target_ip.to_string() + " (" + target_mac.to_string() + ")");
        uint32_t transfer_id = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
        return transmit_file_stream(target_mac, filepath, transfer_id);
    }

    Logger::warn(name_, "ARP Cache MISS for " + target_ip.to_string() + ". Queueing file \"" + filepath + "\" & resolving via ARP.");
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        QueuedPacket qp;
        qp.target_ip = target_ip;
        qp.ethertype = ETHERTYPE_FILE;
        qp.payload.assign(filepath.begin(), filepath.end());
        pending_queue_.push_back(std::move(qp));
    }
    return send_arp_request(target_ip);
}

bool HostNode::transmit_file_stream(const MacAddress& target_mac, const std::string& filepath, uint32_t transfer_id) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::error(name_, "Failed to read file: " + filepath);
        return false;
    }
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(file_size);
    if (file_size > 0) {
        if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
            Logger::error(name_, "Error reading file content: " + filepath);
            return false;
        }
    }
    file.close();

    std::filesystem::path p(filepath);
    std::string filename = p.filename().string();
    if (filename.empty()) filename = "unnamed.bin";

    uint32_t total_chunks = (file_size == 0) ? 1 : static_cast<uint32_t>((file_size + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE);

    // 1. Send FILE_START frame
    FilePacketHeader start_hdr{};
    start_hdr.msg_type = static_cast<uint8_t>(FileMsgType::START);
    start_hdr.transfer_id = transfer_id;
    start_hdr.chunk_idx = 0;
    start_hdr.total_chunks = total_chunks;
    start_hdr.chunk_size = 0;
    start_hdr.file_size = static_cast<uint32_t>(file_size);
    std::strncpy(start_hdr.filename, filename.c_str(), sizeof(start_hdr.filename) - 1);

    EthernetFrame start_frame;
    start_frame.dst = target_mac;
    start_frame.src = mac_;
    start_frame.ethertype = ETHERTYPE_FILE;
    start_frame.payload.resize(sizeof(FilePacketHeader));
    std::memcpy(start_frame.payload.data(), &start_hdr, sizeof(FilePacketHeader));

    Logger::packet("TX", name_ + " -> " + target_mac.to_string() + " [FILE_START] \"" + filename +
                   "\" (" + std::to_string(file_size) + " bytes, " + std::to_string(total_chunks) + " chunks)");
    send_raw_frame(start_frame);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // 2. Stream FILE_DATA chunks
    for (uint32_t idx = 0; idx < total_chunks; ++idx) {
        size_t offset = idx * FILE_CHUNK_SIZE;
        size_t chunk_len = 0;
        if (offset < static_cast<size_t>(file_size)) {
            chunk_len = std::min(FILE_CHUNK_SIZE, static_cast<size_t>(file_size) - offset);
        }

        FilePacketHeader data_hdr{};
        data_hdr.msg_type = static_cast<uint8_t>(FileMsgType::DATA);
        data_hdr.transfer_id = transfer_id;
        data_hdr.chunk_idx = idx;
        data_hdr.total_chunks = total_chunks;
        data_hdr.chunk_size = static_cast<uint32_t>(chunk_len);
        data_hdr.file_size = static_cast<uint32_t>(file_size);
        std::strncpy(data_hdr.filename, filename.c_str(), sizeof(data_hdr.filename) - 1);

        EthernetFrame data_frame;
        data_frame.dst = target_mac;
        data_frame.src = mac_;
        data_frame.ethertype = ETHERTYPE_FILE;
        data_frame.payload.resize(sizeof(FilePacketHeader) + chunk_len);
        std::memcpy(data_frame.payload.data(), &data_hdr, sizeof(FilePacketHeader));
        if (chunk_len > 0) {
            std::memcpy(data_frame.payload.data() + sizeof(FilePacketHeader), buffer.data() + offset, chunk_len);
        }

        send_raw_frame(data_frame);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    // 3. Send FILE_END frame
    FilePacketHeader end_hdr{};
    end_hdr.msg_type = static_cast<uint8_t>(FileMsgType::END);
    end_hdr.transfer_id = transfer_id;
    end_hdr.chunk_idx = total_chunks;
    end_hdr.total_chunks = total_chunks;
    end_hdr.chunk_size = 0;
    end_hdr.file_size = static_cast<uint32_t>(file_size);
    std::strncpy(end_hdr.filename, filename.c_str(), sizeof(end_hdr.filename) - 1);

    EthernetFrame end_frame;
    end_frame.dst = target_mac;
    end_frame.src = mac_;
    end_frame.ethertype = ETHERTYPE_FILE;
    end_frame.payload.resize(sizeof(FilePacketHeader));
    std::memcpy(end_frame.payload.data(), &end_hdr, sizeof(FilePacketHeader));

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    send_raw_frame(end_frame);

    Logger::success(name_, "File transfer completed: \"" + filename + "\" -> " + target_mac.to_string());
    return true;
}

void HostNode::handle_file_packet(const EthernetFrame& frame) {
    if (frame.payload.size() < sizeof(FilePacketHeader)) {
        return;
    }
    const FilePacketHeader* hdr = reinterpret_cast<const FilePacketHeader*>(frame.payload.data());
    uint32_t tid = hdr->transfer_id;

    std::lock_guard<std::mutex> lock(file_mutex_);
    auto msg_type = static_cast<FileMsgType>(hdr->msg_type);

    if (msg_type == FileMsgType::START) {
        FileTransferSession session;
        session.transfer_id = tid;
        session.filename = hdr->filename;
        session.file_size = hdr->file_size;
        session.total_chunks = hdr->total_chunks;
        session.received_chunks = 0;
        session.chunks.resize(hdr->total_chunks);
        session.chunk_received.assign(hdr->total_chunks, false);
        rx_file_sessions_[tid] = session;

        Logger::packet("RX", name_ + " <- [FILE_START] Incoming file \"" + session.filename +
                       "\" (" + std::to_string(session.file_size) + " bytes, " +
                       std::to_string(session.total_chunks) + " chunks) from " + frame.src.to_string());
    } else if (msg_type == FileMsgType::DATA) {
        auto it = rx_file_sessions_.find(tid);
        if (it == rx_file_sessions_.end()) {
            return;
        }
        uint32_t cidx = hdr->chunk_idx;
        if (cidx < it->second.total_chunks && !it->second.chunk_received[cidx]) {
            it->second.chunks[cidx].assign(frame.payload.begin() + sizeof(FilePacketHeader), frame.payload.end());
            it->second.chunk_received[cidx] = true;
            it->second.received_chunks++;
        }
    } else if (msg_type == FileMsgType::END) {
        auto it = rx_file_sessions_.find(tid);
        if (it == rx_file_sessions_.end()) {
            return;
        }

        std::string host_dir = rx_directory_ + "/" + name_;
        std::error_code ec;
        std::filesystem::create_directories(host_dir, ec);

        std::string out_filepath = host_dir + "/" + it->second.filename;
        std::ofstream outfile(out_filepath, std::ios::binary);
        if (outfile.is_open()) {
            for (const auto& chunk : it->second.chunks) {
                outfile.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            }
            outfile.close();
            Logger::success(name_, "File \"" + it->second.filename + "\" (" +
                            std::to_string(it->second.file_size) + " bytes) saved to: " + out_filepath);
        } else {
            Logger::error(name_, "Failed to write received file to: " + out_filepath);
        }

        rx_file_sessions_.erase(it);
    }
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
