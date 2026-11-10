#include "frame.hpp"
#include <cstring>
#include <arpa/inet.h>

namespace cdma {

const uint8_t FrameCodec::MAGIC[4] = {0xCD, 0x31, 0x34, 0x4D};

std::vector<uint8_t> FrameCodec::serialize_packet(const Packet& pkt) {
    std::vector<uint8_t> buf(sizeof(FrameHeader) + pkt.payload.size());
    FrameHeader* hdr = reinterpret_cast<FrameHeader*>(buf.data());

    std::memcpy(hdr->magic, MAGIC, 4);
    hdr->type = static_cast<uint8_t>(pkt.type);
    hdr->reserved = 0;
    hdr->station_id = htons(pkt.station_id);
    hdr->round_seq = htonl(pkt.round_seq);
    hdr->payload_len = htonl(static_cast<uint32_t>(pkt.payload.size()));

    if (!pkt.payload.empty()) {
        std::memcpy(buf.data() + sizeof(FrameHeader), pkt.payload.data(), pkt.payload.size());
    }

    return buf;
}

bool FrameCodec::deserialize_header(const uint8_t* data, size_t len, FrameHeader& out_hdr) {
    if (len < sizeof(FrameHeader)) return false;

    const FrameHeader* in_hdr = reinterpret_cast<const FrameHeader*>(data);
    if (std::memcmp(in_hdr->magic, MAGIC, 4) != 0) {
        return false;
    }

    std::memcpy(out_hdr.magic, in_hdr->magic, 4);
    out_hdr.type = in_hdr->type;
    out_hdr.reserved = in_hdr->reserved;
    out_hdr.station_id = ntohs(in_hdr->station_id);
    out_hdr.round_seq = ntohl(in_hdr->round_seq);
    out_hdr.payload_len = ntohl(in_hdr->payload_len);

    return true;
}

bool FrameCodec::parse_packet(const std::vector<uint8_t>& raw_bytes, Packet& out_pkt) {
    if (raw_bytes.size() < sizeof(FrameHeader)) return false;

    FrameHeader hdr;
    if (!deserialize_header(raw_bytes.data(), raw_bytes.size(), hdr)) {
        return false;
    }

    if (raw_bytes.size() < sizeof(FrameHeader) + hdr.payload_len) {
        return false;
    }

    out_pkt.type = static_cast<MsgType>(hdr.type);
    out_pkt.station_id = hdr.station_id;
    out_pkt.round_seq = hdr.round_seq;
    out_pkt.payload.assign(
        raw_bytes.begin() + sizeof(FrameHeader),
        raw_bytes.begin() + sizeof(FrameHeader) + hdr.payload_len
    );

    return true;
}

std::vector<uint8_t> FrameCodec::encode_register_resp(const RegisterRespData& data) {
    std::vector<uint8_t> payload(6 + data.walsh_code.size());
    uint16_t n_id = htons(data.assigned_id);
    uint16_t n_len = htons(data.code_len);
    uint16_t n_total = htons(data.total_stations);

    std::memcpy(payload.data(), &n_id, 2);
    std::memcpy(payload.data() + 2, &n_len, 2);
    std::memcpy(payload.data() + 4, &n_total, 2);

    for (size_t i = 0; i < data.walsh_code.size(); ++i) {
        payload[6 + i] = static_cast<uint8_t>(data.walsh_code[i]);
    }

    return payload;
}

bool FrameCodec::decode_register_resp(const std::vector<uint8_t>& payload, RegisterRespData& out_data) {
    if (payload.size() < 6) return false;

    uint16_t n_id, n_len, n_total;
    std::memcpy(&n_id, payload.data(), 2);
    std::memcpy(&n_len, payload.data() + 2, 2);
    std::memcpy(&n_total, payload.data() + 4, 2);

    out_data.assigned_id = ntohs(n_id);
    out_data.code_len = ntohs(n_len);
    out_data.total_stations = ntohs(n_total);

    if (payload.size() < 6 + out_data.code_len) return false;

    out_data.walsh_code.resize(out_data.code_len);
    for (size_t i = 0; i < out_data.code_len; ++i) {
        out_data.walsh_code[i] = static_cast<int8_t>(payload[6 + i]);
    }

    return true;
}

std::vector<uint8_t> FrameCodec::encode_config_sync(const ConfigSyncData& data) {
    size_t total_elements = static_cast<size_t>(data.code_len) * data.code_len;
    std::vector<uint8_t> payload(4 + total_elements);

    uint16_t n_len = htons(data.code_len);
    uint16_t n_total = htons(data.total_stations);
    std::memcpy(payload.data(), &n_len, 2);
    std::memcpy(payload.data() + 2, &n_total, 2);

    size_t offset = 4;
    for (size_t i = 0; i < data.walsh_matrix.size(); ++i) {
        for (size_t j = 0; j < data.walsh_matrix[i].size(); ++j) {
            payload[offset++] = static_cast<uint8_t>(data.walsh_matrix[i][j]);
        }
    }

    return payload;
}

bool FrameCodec::decode_config_sync(const std::vector<uint8_t>& payload, ConfigSyncData& out_data) {
    if (payload.size() < 4) return false;

    uint16_t n_len, n_total;
    std::memcpy(&n_len, payload.data(), 2);
    std::memcpy(&n_total, payload.data() + 2, 2);

    out_data.code_len = ntohs(n_len);
    out_data.total_stations = ntohs(n_total);

    size_t total_elements = static_cast<size_t>(out_data.code_len) * out_data.code_len;
    if (payload.size() < 4 + total_elements) return false;

    out_data.walsh_matrix.assign(
        out_data.code_len,
        std::vector<int8_t>(out_data.code_len, 0)
    );

    size_t offset = 4;
    for (size_t i = 0; i < out_data.code_len; ++i) {
        for (size_t j = 0; j < out_data.code_len; ++j) {
            out_data.walsh_matrix[i][j] = static_cast<int8_t>(payload[offset++]);
        }
    }

    return true;
}

std::vector<uint8_t> FrameCodec::encode_chip_transmit(const ChipTransmitData& data) {
    std::vector<uint8_t> payload(8 + data.chips.size());

    uint16_t n_id = htons(data.station_id);
    uint32_t n_seq = htonl(data.round_seq);
    uint16_t n_chips = htons(static_cast<uint16_t>(data.chips.size()));

    std::memcpy(payload.data(), &n_id, 2);
    std::memcpy(payload.data() + 2, &n_seq, 4);
    std::memcpy(payload.data() + 6, &n_chips, 2);

    for (size_t i = 0; i < data.chips.size(); ++i) {
        payload[8 + i] = static_cast<uint8_t>(data.chips[i]);
    }

    return payload;
}

bool FrameCodec::decode_chip_transmit(const std::vector<uint8_t>& payload, ChipTransmitData& out_data) {
    if (payload.size() < 8) return false;

    uint16_t n_id, n_chips;
    uint32_t n_seq;

    std::memcpy(&n_id, payload.data(), 2);
    std::memcpy(&n_seq, payload.data() + 2, 4);
    std::memcpy(&n_chips, payload.data() + 6, 2);

    out_data.station_id = ntohs(n_id);
    out_data.round_seq = ntohl(n_seq);
    uint16_t count = ntohs(n_chips);

    if (payload.size() < 8 + count) return false;

    out_data.chips.resize(count);
    for (size_t i = 0; i < count; ++i) {
        out_data.chips[i] = static_cast<int8_t>(payload[8 + i]);
    }

    return true;
}

std::vector<uint8_t> FrameCodec::encode_channel_broadcast(const ChannelBroadcastData& data) {
    std::vector<uint8_t> payload(6 + data.composite_signal.size() * 4);

    uint32_t n_seq = htonl(data.round_seq);
    uint16_t n_len = htons(data.code_len);

    std::memcpy(payload.data(), &n_seq, 4);
    std::memcpy(payload.data() + 4, &n_len, 2);

    size_t offset = 6;
    for (int32_t val : data.composite_signal) {
        uint32_t n_val = htonl(static_cast<uint32_t>(val));
        std::memcpy(payload.data() + offset, &n_val, 4);
        offset += 4;
    }

    return payload;
}

bool FrameCodec::decode_channel_broadcast(const std::vector<uint8_t>& payload, ChannelBroadcastData& out_data) {
    if (payload.size() < 6) return false;

    uint32_t n_seq;
    uint16_t n_len;

    std::memcpy(&n_seq, payload.data(), 4);
    std::memcpy(&n_len, payload.data() + 4, 2);

    out_data.round_seq = ntohl(n_seq);
    out_data.code_len = ntohs(n_len);

    if (payload.size() < 6 + out_data.code_len * 4) return false;

    out_data.composite_signal.resize(out_data.code_len);
    size_t offset = 6;
    for (size_t i = 0; i < out_data.code_len; ++i) {
        uint32_t n_val;
        std::memcpy(&n_val, payload.data() + offset, 4);
        out_data.composite_signal[i] = static_cast<int32_t>(ntohl(n_val));
        offset += 4;
    }

    return true;
}

} // namespace cdma
