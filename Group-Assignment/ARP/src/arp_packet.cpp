#include "arp_packet.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace arp {

// ---------------- EthernetFrame ----------------

std::vector<uint8_t> EthernetFrame::serialize() const {
    std::vector<uint8_t> buffer(sizeof(EthernetHeader) + payload.size());
    EthernetHeader* hdr = reinterpret_cast<EthernetHeader*>(buffer.data());

    std::memcpy(hdr->dest_mac, dst.bytes.data(), 6);
    std::memcpy(hdr->src_mac, src.bytes.data(), 6);
    hdr->ethertype = htons(ethertype);

    if (!payload.empty()) {
        std::memcpy(buffer.data() + sizeof(EthernetHeader), payload.data(), payload.size());
    }
    return buffer;
}

bool EthernetFrame::deserialize(const uint8_t* data, size_t len, EthernetFrame& out_frame) {
    if (!data || len < sizeof(EthernetHeader)) {
        return false;
    }
    const EthernetHeader* hdr = reinterpret_cast<const EthernetHeader*>(data);
    out_frame.dst = MacAddress(hdr->dest_mac);
    out_frame.src = MacAddress(hdr->src_mac);
    out_frame.ethertype = ntohs(hdr->ethertype);

    size_t payload_len = len - sizeof(EthernetHeader);
    out_frame.payload.resize(payload_len);
    if (payload_len > 0) {
        std::memcpy(out_frame.payload.data(), data + sizeof(EthernetHeader), payload_len);
    }
    return true;
}

// ---------------- ArpPacket ----------------

ArpPacket ArpPacket::make_request(const MacAddress& sender_mac,
                                  const IpAddress& sender_ip,
                                  const IpAddress& target_ip) {
    ArpPacket pkt;
    pkt.htype = 1;
    pkt.ptype = ETHERTYPE_IPV4;
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.opcode = ArpOpcode::REQUEST;
    pkt.sender_mac = sender_mac;
    pkt.sender_ip = sender_ip;
    pkt.target_mac = MacAddress::zero(); // Unknown in request
    pkt.target_ip = target_ip;
    return pkt;
}

ArpPacket ArpPacket::make_reply(const MacAddress& sender_mac,
                                const IpAddress& sender_ip,
                                const MacAddress& target_mac,
                                const IpAddress& target_ip) {
    ArpPacket pkt;
    pkt.htype = 1;
    pkt.ptype = ETHERTYPE_IPV4;
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.opcode = ArpOpcode::REPLY;
    pkt.sender_mac = sender_mac;
    pkt.sender_ip = sender_ip;
    pkt.target_mac = target_mac;
    pkt.target_ip = target_ip;
    return pkt;
}

ArpPacket ArpPacket::make_gratuitous(const MacAddress& sender_mac,
                                     const IpAddress& ip) {
    ArpPacket pkt;
    pkt.htype = 1;
    pkt.ptype = ETHERTYPE_IPV4;
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.opcode = ArpOpcode::REQUEST;
    pkt.sender_mac = sender_mac;
    pkt.sender_ip = ip;
    pkt.target_mac = MacAddress::broadcast(); // or zero
    pkt.target_ip = ip; // SPA == TPA
    return pkt;
}

std::vector<uint8_t> ArpPacket::serialize() const {
    std::vector<uint8_t> buffer(sizeof(ArpRawHeader));
    ArpRawHeader* hdr = reinterpret_cast<ArpRawHeader*>(buffer.data());

    hdr->htype = htons(htype);
    hdr->ptype = htons(ptype);
    hdr->hlen = hlen;
    hdr->plen = plen;
    hdr->opcode = htons(static_cast<uint16_t>(opcode));

    std::memcpy(hdr->sender_mac, sender_mac.bytes.data(), 6);
    hdr->sender_ip = sender_ip.to_network_order();
    std::memcpy(hdr->target_mac, target_mac.bytes.data(), 6);
    hdr->target_ip = target_ip.to_network_order();

    return buffer;
}

bool ArpPacket::deserialize(const uint8_t* data, size_t len, ArpPacket& out_pkt) {
    if (!data || len < sizeof(ArpRawHeader)) {
        return false;
    }
    const ArpRawHeader* hdr = reinterpret_cast<const ArpRawHeader*>(data);

    out_pkt.htype = ntohs(hdr->htype);
    out_pkt.ptype = ntohs(hdr->ptype);
    out_pkt.hlen = hdr->hlen;
    out_pkt.plen = hdr->plen;
    out_pkt.opcode = static_cast<ArpOpcode>(ntohs(hdr->opcode));

    if (out_pkt.hlen != 6 || out_pkt.plen != 4) {
        return false;
    }

    out_pkt.sender_mac = MacAddress(hdr->sender_mac);
    out_pkt.sender_ip = IpAddress(ntohl(hdr->sender_ip));
    out_pkt.target_mac = MacAddress(hdr->target_mac);
    out_pkt.target_ip = IpAddress(ntohl(hdr->target_ip));

    return true;
}

bool ArpPacket::is_gratuitous() const {
    return sender_ip == target_ip;
}

std::string ArpPacket::to_string() const {
    std::ostringstream oss;
    std::string op_str = (opcode == ArpOpcode::REQUEST) ? "REQUEST" :
                         (opcode == ArpOpcode::REPLY)   ? "REPLY" : "OTHER";
    oss << "ARP " << op_str
        << " [Sender: " << sender_ip << " / " << sender_mac
        << " -> Target: " << target_ip << " / " << target_mac << "]";
    return oss.str();
}

std::string ArpPacket::disassemble_summary() const {
    std::ostringstream oss;
    if (opcode == ArpOpcode::REQUEST) {
        if (is_gratuitous()) {
            oss << "Gratuitous ARP (Announcement/Probe) for " << sender_ip
                << " from " << sender_mac;
        } else {
            oss << "Who has " << target_ip << "? Tell " << sender_ip << " (" << sender_mac << ")";
        }
    } else if (opcode == ArpOpcode::REPLY) {
        oss << sender_ip << " is at " << sender_mac << " (Reply to " << target_ip << ")";
    } else {
        oss << "ARP Opcode=" << static_cast<int>(opcode);
    }
    return oss.str();
}

} // namespace arp
