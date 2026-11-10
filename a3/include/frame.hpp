#ifndef A3_FRAME_HPP
#define A3_FRAME_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace csma {

enum class ChannelState : uint8_t {
    IDLE = 0,
    BUSY = 1,
    COLLISION = 2
};

enum class ProtocolType : uint8_t {
    NON_PERSISTENT = 0,
    ONE_PERSISTENT = 1,
    P_PERSISTENT = 2,
    CSMA_CD = 3
};

enum class MsgType : uint8_t {
    MSG_SENSE_REQ = 1,
    MSG_SENSE_RESP = 2,
    MSG_START_TX = 3,
    MSG_CHECK_CD = 4,
    MSG_END_TX = 5,
    MSG_JAM = 6,
    MSG_STATS_REQ = 7,
    MSG_STATS_RESP = 8,
    MSG_RESET_REQ = 9,
    MSG_RESET_RESP = 10
};

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t msg_type;
    uint32_t station_id;
    uint32_t frame_seq;
    uint8_t channel_state;
    uint32_t duration_ms;
    uint8_t collided;
    uint32_t extra_info;
    uint64_t timestamp_us;
    uint32_t payload_len;
};
#pragma pack(pop)

struct Message {
    MessageHeader header{};
    std::string payload;

    Message() {
        header.msg_type = 0;
        header.station_id = 0;
        header.frame_seq = 0;
        header.channel_state = 0;
        header.duration_ms = 0;
        header.collided = 0;
        header.extra_info = 0;
        header.timestamp_us = 0;
        header.payload_len = 0;
    }
};

std::string channel_state_to_str(ChannelState state);
std::string protocol_type_to_str(ProtocolType proto);
std::string msg_type_to_str(MsgType type);

std::vector<uint8_t> serialize_message(const Message& msg);
bool deserialize_message(const uint8_t* data, size_t len, Message& msg);

} // namespace csma

#endif // A3_FRAME_HPP
