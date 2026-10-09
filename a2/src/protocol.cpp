#include "protocol.hpp"
#include <iomanip>
#include <iostream>
#include <algorithm>

using namespace std;

namespace net {

const char* protocol_type_to_string(ProtocolType type) {
    switch (type) {
        case ProtocolType::STOP_AND_WAIT:     return "Stop-and-Wait";
        case ProtocolType::GO_BACK_N:         return "Go-Back-N";
        case ProtocolType::SELECTIVE_REPEAT:  return "Selective-Repeat";
        default:                              return "Unknown";
    }
}

ProtocolType string_to_protocol_type(const string& str) {
    string s = str;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "sw" || s == "stop-and-wait" || s == "stop_and_wait") {
        return ProtocolType::STOP_AND_WAIT;
    } else if (s == "gbn" || s == "go-back-n" || s == "go_back_n") {
        return ProtocolType::GO_BACK_N;
    } else if (s == "sr" || s == "selective-repeat" || s == "selective_repeat") {
        return ProtocolType::SELECTIVE_REPEAT;
    }
    return ProtocolType::STOP_AND_WAIT;
}

void ProtocolStats::print_sender(ostream& os) const {
    os << "\n================= SENDER PERFORMANCE METRICS =================\n";
    os << "  Original Data Frames Sent: " << original_data_frames << "\n";
    os << "  Total Frames Transmitted:  " << total_transmissions << "\n";
    os << "  Retransmissions:           " << retransmissions << "\n";
    os << "  ACKs Processed (from Recv):" << acks_sent_or_received << "\n";
    if (naks_sent_or_received > 0) {
        os << "  NAKs Processed:            " << naks_sent_or_received << "\n";
    }
    os << "  Timeouts Occurred:         " << timeouts << "\n";
    os << "  Corrupted Frames Detected: " << corrupted_frames << "\n";
    os << "  Total Wire Bytes Sent:     " << total_wire_bytes_sent << " bytes\n";
    os << "  Payload Sent (Delivered):  " << payload_bytes_delivered << " bytes\n";
    os << "  Total Elapsed Time:        " << fixed << setprecision(2)
       << elapsed_time_ms << " ms\n";
    os << "  Average Round-Trip Time:   " << fixed << setprecision(2)
       << avg_rtt_ms << " ms\n";
    os << "  Final Timeout (RTO):       " << fixed << setprecision(2)
       << final_rto_ms << " ms\n";
    os << "  Protocol Efficiency:       " << fixed << setprecision(4)
       << (get_efficiency() * 100.0) << " %\n";
    os << "  Effective Throughput:      " << fixed << setprecision(2)
       << get_throughput_kbps() << " kbps\n";
    os << "===============================================================\n";
}

void ProtocolStats::print_receiver(const string& output_file, ostream& os) const {
    os << "\n================ RECEIVER PERFORMANCE METRICS ================\n";
    os << "  Total Data Frames Received:" << total_transmissions << "\n";
    os << "  Valid Frames Accepted:     " << frames_accepted << "\n";
    os << "  Duplicate Frames Discarded:" << duplicate_frames << "\n";
    os << "  Corrupted Frames Detected: " << corrupted_frames << "\n";
    os << "  ACKs Sent to Sender:       " << acks_sent_or_received << "\n";
    if (naks_sent_or_received > 0) {
        os << "  NAKs Sent to Sender:       " << naks_sent_or_received << "\n";
    }
    os << "  Payload Written to File:   " << payload_bytes_delivered << " bytes\n";
    if (!output_file.empty()) {
        os << "  Saved Output File:         " << output_file << "\n";
    }
    os << "===============================================================\n";
}

} // namespace net
