#include "frame.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <arpa/inet.h>

using namespace std;

namespace net {

const char* frame_type_to_string(FrameType type) {
    switch (type) {
        case FrameType::DATA: return "DATA";
        case FrameType::ACK:  return "ACK";
        case FrameType::NAK:  return "NAK";
        case FrameType::FIN:  return "FIN";
        default: return "UNKNOWN";
    }
}

Frame::Frame() : fcs(0) {
    memset(&header, 0, sizeof(header));
}

Frame Frame::create_data(uint8_t seq, const vector<uint8_t>& data,
                         ErrorDetectionMethod method,
                         const uint8_t* src, const uint8_t* dest) {
    Frame f;
    if (src) memcpy(f.header.src_mac, src, MAC_LEN);
    else memset(f.header.src_mac, 0xAA, MAC_LEN);

    if (dest) memcpy(f.header.dest_mac, dest, MAC_LEN);
    else memset(f.header.dest_mac, 0xBB, MAC_LEN);

    f.header.seq_num = seq;
    f.header.type = static_cast<uint8_t>(FrameType::DATA);
    f.header.length = static_cast<uint16_t>(data.size());
    f.payload = data;
    f.update_fcs(method);
    return f;
}

Frame Frame::create_ack(uint8_t ack_seq, ErrorDetectionMethod method,
                        const uint8_t* src, const uint8_t* dest) {
    Frame f;
    if (src) memcpy(f.header.src_mac, src, MAC_LEN);
    else memset(f.header.src_mac, 0xBB, MAC_LEN);

    if (dest) memcpy(f.header.dest_mac, dest, MAC_LEN);
    else memset(f.header.dest_mac, 0xAA, MAC_LEN);

    f.header.seq_num = ack_seq;
    f.header.type = static_cast<uint8_t>(FrameType::ACK);
    f.header.length = 0;
    f.update_fcs(method);
    return f;
}

Frame Frame::create_nak(uint8_t nak_seq, ErrorDetectionMethod method,
                        const uint8_t* src, const uint8_t* dest) {
    Frame f;
    if (src) memcpy(f.header.src_mac, src, MAC_LEN);
    else memset(f.header.src_mac, 0xBB, MAC_LEN);

    if (dest) memcpy(f.header.dest_mac, dest, MAC_LEN);
    else memset(f.header.dest_mac, 0xAA, MAC_LEN);

    f.header.seq_num = nak_seq;
    f.header.type = static_cast<uint8_t>(FrameType::NAK);
    f.header.length = 0;
    f.update_fcs(method);
    return f;
}

Frame Frame::create_fin(uint8_t seq, ErrorDetectionMethod method,
                        const uint8_t* src, const uint8_t* dest) {
    Frame f;
    if (src) memcpy(f.header.src_mac, src, MAC_LEN);
    else memset(f.header.src_mac, 0xAA, MAC_LEN);

    if (dest) memcpy(f.header.dest_mac, dest, MAC_LEN);
    else memset(f.header.dest_mac, 0xBB, MAC_LEN);

    f.header.seq_num = seq;
    f.header.type = static_cast<uint8_t>(FrameType::FIN);
    f.header.length = 0;
    f.update_fcs(method);
    return f;
}

// serialize frame to network wire format
vector<uint8_t> Frame::serialize() const {
    size_t total_size = HEADER_SIZE + payload.size() + TRAILER_SIZE;
    vector<uint8_t> buffer(total_size);

    FrameHeader net_hdr = header;
    net_hdr.length = htons(header.length);

    memcpy(buffer.data(), &net_hdr, HEADER_SIZE);
    if (!payload.empty()) {
        memcpy(buffer.data() + HEADER_SIZE, payload.data(), payload.size());
    }

    uint32_t net_fcs = htonl(fcs);
    memcpy(buffer.data() + HEADER_SIZE + payload.size(), &net_fcs, TRAILER_SIZE);

    return buffer;
}

// deserialize wire bytes into frame
bool Frame::deserialize(const uint8_t* buffer, size_t len, Frame& out_frame) {
    if (len < HEADER_SIZE + TRAILER_SIZE) {
        return false;
    }

    FrameHeader net_hdr;
    memcpy(&net_hdr, buffer, HEADER_SIZE);

    out_frame.header = net_hdr;
    out_frame.header.length = ntohs(net_hdr.length);

    if (len < HEADER_SIZE + out_frame.header.length + TRAILER_SIZE) {
        return false;
    }

    out_frame.payload.resize(out_frame.header.length);
    if (out_frame.header.length > 0) {
        memcpy(out_frame.payload.data(), buffer + HEADER_SIZE, out_frame.header.length);
    }

    uint32_t net_fcs = 0;
    memcpy(&net_fcs, buffer + HEADER_SIZE + out_frame.header.length, TRAILER_SIZE);
    out_frame.fcs = ntohl(net_fcs);

    return true;
}

void Frame::update_fcs(ErrorDetectionMethod method) {
    FrameHeader net_hdr = header;
    net_hdr.length = htons(header.length);

    vector<uint8_t> check_buf(HEADER_SIZE + payload.size());
    memcpy(check_buf.data(), &net_hdr, HEADER_SIZE);
    if (!payload.empty()) {
        memcpy(check_buf.data() + HEADER_SIZE, payload.data(), payload.size());
    }

    fcs = compute_fcs(method, check_buf.data(), check_buf.size());
}

bool Frame::verify_fcs(ErrorDetectionMethod method) const {
    FrameHeader net_hdr = header;
    net_hdr.length = htons(header.length);

    vector<uint8_t> check_buf(HEADER_SIZE + payload.size());
    memcpy(check_buf.data(), &net_hdr, HEADER_SIZE);
    if (!payload.empty()) {
        memcpy(check_buf.data() + HEADER_SIZE, payload.data(), payload.size());
    }

    return net::verify_fcs(method, check_buf.data(), check_buf.size(), fcs);
}

string Frame::to_string() const {
    ostringstream oss;
    oss << "[" << frame_type_to_string(static_cast<FrameType>(header.type))
        << " | Seq=" << static_cast<int>(header.seq_num)
        << " | Len=" << header.length
        << " | FCS=0x" << hex << uppercase << setfill('0') << setw(8) << fcs
        << dec << "]";
    return oss.str();
}

} // namespace net
