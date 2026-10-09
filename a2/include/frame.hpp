#ifndef A2_FRAME_HPP
#define A2_FRAME_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "crc.hpp"

namespace net {

using std::string;
using std::vector;

constexpr size_t MAC_LEN = 6;
constexpr size_t MIN_PAYLOAD_LEN = 46;
constexpr size_t MAX_PAYLOAD_LEN = 1500;
constexpr size_t DEFAULT_PAYLOAD_LEN = 64;

enum class FrameType : uint8_t {
    DATA = 0,
    ACK  = 1,
    NAK  = 2,
    FIN  = 3
};

const char* frame_type_to_string(FrameType type);

#pragma pack(push, 1)
// packed frame header
struct FrameHeader {
    uint8_t  src_mac[MAC_LEN];
    uint8_t  dest_mac[MAC_LEN];
    uint16_t length;
    uint8_t  seq_num;
    uint8_t  type;
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(FrameHeader);
constexpr size_t TRAILER_SIZE = sizeof(uint32_t);

// complete frame structure
struct Frame {
    FrameHeader header;
    vector<uint8_t> payload;
    uint32_t fcs;

    Frame();
    static Frame create_data(uint8_t seq, const vector<uint8_t>& data,
                             ErrorDetectionMethod method = ErrorDetectionMethod::CRC_32,
                             const uint8_t* src = nullptr, const uint8_t* dest = nullptr);

    static Frame create_ack(uint8_t ack_seq,
                            ErrorDetectionMethod method = ErrorDetectionMethod::CRC_32,
                            const uint8_t* src = nullptr, const uint8_t* dest = nullptr);

    static Frame create_nak(uint8_t nak_seq,
                            ErrorDetectionMethod method = ErrorDetectionMethod::CRC_32,
                            const uint8_t* src = nullptr, const uint8_t* dest = nullptr);

    static Frame create_fin(uint8_t seq,
                            ErrorDetectionMethod method = ErrorDetectionMethod::CRC_32,
                            const uint8_t* src = nullptr, const uint8_t* dest = nullptr);

    vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* buffer, size_t len, Frame& out_frame);
    bool verify_fcs(ErrorDetectionMethod method) const;
    void update_fcs(ErrorDetectionMethod method);
    string to_string() const;
};

} // namespace net

#endif // A2_FRAME_HPP
