#ifndef A2_PROTOCOL_HPP
#define A2_PROTOCOL_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include "frame.hpp"
#include "socket.hpp"
#include "channel.hpp"
#include "timer.hpp"

namespace net {

using std::string;
using std::ostream;
using std::cout;

enum class ProtocolType {
    STOP_AND_WAIT,
    GO_BACK_N,
    SELECTIVE_REPEAT
};

const char* protocol_type_to_string(ProtocolType type);
ProtocolType string_to_protocol_type(const string& str);

// protocol performance metrics
struct ProtocolStats {
    uint64_t original_data_frames = 0;
    uint64_t total_transmissions = 0;
    uint64_t retransmissions = 0;
    uint64_t acks_sent_or_received = 0;
    uint64_t naks_sent_or_received = 0;
    uint64_t timeouts = 0;
    uint64_t corrupted_frames = 0;
    uint64_t dropped_frames = 0;
    uint64_t total_wire_bytes_sent = 0;
    uint64_t payload_bytes_delivered = 0;
    double   elapsed_time_ms = 0.0;
    double   avg_rtt_ms = 0.0;
    double   final_rto_ms = 0.0;

    uint64_t duplicate_frames = 0;
    uint64_t frames_accepted = 0;

    double get_efficiency() const {
        if (total_wire_bytes_sent == 0) return 0.0;
        return static_cast<double>(payload_bytes_delivered) / static_cast<double>(total_wire_bytes_sent);
    }

    double get_throughput_kbps() const {
        if (elapsed_time_ms <= 0.0) return 0.0;
        return (payload_bytes_delivered * 8.0) / elapsed_time_ms;
    }

    void print_sender(ostream& os = cout) const;
    void print_receiver(const string& output_file = "", ostream& os = cout) const;
    void print(ostream& os = cout) const { print_sender(os); }
};

// protocol configuration
struct ProtocolConfig {
    ProtocolType protocol = ProtocolType::STOP_AND_WAIT;
    int window_size = 4;
    size_t payload_size = DEFAULT_PAYLOAD_LEN;
    ErrorDetectionMethod fcs_method = ErrorDetectionMethod::CRC_32;
    double initial_timeout_ms = 150.0;
    double min_timeout_ms = 40.0;
    double max_timeout_ms = 2000.0;
    bool verbose = true;
};

} // namespace net

#endif // A2_PROTOCOL_HPP
