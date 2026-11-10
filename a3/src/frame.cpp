#include "frame.hpp"
#include <cstring>

namespace csma {

std::string channel_state_to_str(ChannelState state) {
    switch (state) {
        case ChannelState::IDLE: return "IDLE";
        case ChannelState::BUSY: return "BUSY";
        case ChannelState::COLLISION: return "COLLISION";
        default: return "UNKNOWN";
    }
}

std::string protocol_type_to_str(ProtocolType proto) {
    switch (proto) {
        case ProtocolType::NON_PERSISTENT: return "Non-Persistent CSMA";
        case ProtocolType::ONE_PERSISTENT: return "1-Persistent CSMA";
        case ProtocolType::P_PERSISTENT: return "p-Persistent CSMA";
        case ProtocolType::CSMA_CD: return "CSMA/CD";
        default: return "UNKNOWN";
    }
}

std::string msg_type_to_str(MsgType type) {
    switch (type) {
        case MsgType::MSG_SENSE_REQ: return "SENSE_REQ";
        case MsgType::MSG_SENSE_RESP: return "SENSE_RESP";
        case MsgType::MSG_START_TX: return "START_TX";
        case MsgType::MSG_CHECK_CD: return "CHECK_CD";
        case MsgType::MSG_END_TX: return "END_TX";
        case MsgType::MSG_JAM: return "JAM";
        case MsgType::MSG_STATS_REQ: return "STATS_REQ";
        case MsgType::MSG_STATS_RESP: return "STATS_RESP";
        case MsgType::MSG_RESET_REQ: return "RESET_REQ";
        case MsgType::MSG_RESET_RESP: return "RESET_RESP";
        default: return "UNKNOWN";
    }
}

std::vector<uint8_t> serialize_message(const Message& msg) {
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + msg.payload.size());
    MessageHeader hdr = msg.header;
    hdr.payload_len = static_cast<uint32_t>(msg.payload.size());
    
    std::memcpy(buffer.data(), &hdr, sizeof(MessageHeader));
    if (!msg.payload.empty()) {
        std::memcpy(buffer.data() + sizeof(MessageHeader), msg.payload.data(), msg.payload.size());
    }
    return buffer;
}

bool deserialize_message(const uint8_t* data, size_t len, Message& msg) {
    if (len < sizeof(MessageHeader)) {
        return false;
    }
    std::memcpy(&msg.header, data, sizeof(MessageHeader));
    size_t payload_len = msg.header.payload_len;
    if (len < sizeof(MessageHeader) + payload_len) {
        return false;
    }
    if (payload_len > 0) {
        msg.payload.assign(reinterpret_cast<const char*>(data + sizeof(MessageHeader)), payload_len);
    } else {
        msg.payload.clear();
    }
    return true;
}

} // namespace csma
