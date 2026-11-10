#ifndef A4_FRAME_HPP
#define A4_FRAME_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include "walsh.hpp"

namespace cdma {

// Protocol Message Types
enum class MsgType : uint8_t {
    REGISTER_REQ      = 0x10, // Station -> Server: Join CDMA channel
    REGISTER_RESP     = 0x11, // Server -> Station: ID, M, N, assigned Walsh Code
    CONFIG_SYNC       = 0x12, // Server -> Station: Full Walsh Matrix of size M
    SLOT_START        = 0x20, // Server -> Station: Slotted round begin signal
    CHIP_TRANSMIT     = 0x21, // Station -> Server: Modulated chip vector
    CHANNEL_BROADCAST = 0x22, // Server -> Station: Superimposed composite signal S
    DISCONNECT        = 0x30, // Station <-> Server: Graceful exit
    STATUS_ACK        = 0x31  // Generic ACK
};

// Fixed Header Layout (12 bytes total):
// [4 bytes Magic: 0xCD, 0x31, 0x34, 0x4D]
// [1 byte MsgType]
// [1 byte Reserved]
// [2 bytes StationID (uint16_t)]
// [4 bytes RoundSeq (uint32_t)]
// [4 bytes PayloadLen (uint32_t)]
#pragma pack(push, 1)
struct FrameHeader {
    uint8_t  magic[4];     // 0xCD, 0x31, 0x34, 0x4D
    uint8_t  type;         // MsgType
    uint8_t  reserved;
    uint16_t station_id;   // Network byte order
    uint32_t round_seq;    // Network byte order
    uint32_t payload_len;  // Network byte order
};
#pragma pack(pop)

// High-level packet representation
struct Packet {
    MsgType type;
    uint16_t station_id;
    uint32_t round_seq;
    std::vector<uint8_t> payload;

    Packet() : type(MsgType::STATUS_ACK), station_id(0), round_seq(0) {}
    Packet(MsgType t, uint16_t stn_id, uint32_t r_seq, const std::vector<uint8_t>& data = {})
        : type(t), station_id(stn_id), round_seq(r_seq), payload(data) {}
};

// Concrete payloads
struct RegisterRespData {
    uint16_t assigned_id;
    uint16_t code_len;       // M
    uint16_t total_stations; // N
    std::vector<int8_t> walsh_code; // Size M
};

struct ConfigSyncData {
    uint16_t code_len;       // M
    uint16_t total_stations; // N
    std::vector<std::vector<int8_t>> walsh_matrix; // M x M
};

struct ChipTransmitData {
    uint16_t station_id;
    uint32_t round_seq;
    std::vector<int8_t> chips; // Size M
};

struct ChannelBroadcastData {
    uint32_t round_seq;
    uint16_t code_len;
    std::vector<int32_t> composite_signal; // Size M
};

// Serialization & Deserialization Engine
class FrameCodec {
public:
    static const uint8_t MAGIC[4];

    // Encodes packet into binary byte stream
    static std::vector<uint8_t> serialize_packet(const Packet& pkt);

    // Parses a packet from a byte buffer
    static bool deserialize_header(const uint8_t* data, size_t len, FrameHeader& out_hdr);
    static bool parse_packet(const std::vector<uint8_t>& raw_bytes, Packet& out_pkt);

    // Specialized Payload Serializers
    static std::vector<uint8_t> encode_register_resp(const RegisterRespData& data);
    static bool decode_register_resp(const std::vector<uint8_t>& payload, RegisterRespData& out_data);

    static std::vector<uint8_t> encode_config_sync(const ConfigSyncData& data);
    static bool decode_config_sync(const std::vector<uint8_t>& payload, ConfigSyncData& out_data);

    static std::vector<uint8_t> encode_chip_transmit(const ChipTransmitData& data);
    static bool decode_chip_transmit(const std::vector<uint8_t>& payload, ChipTransmitData& out_data);

    static std::vector<uint8_t> encode_channel_broadcast(const ChannelBroadcastData& data);
    static bool decode_channel_broadcast(const std::vector<uint8_t>& payload, ChannelBroadcastData& out_data);
};

} // namespace cdma

#endif // A4_FRAME_HPP
