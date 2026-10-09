#ifndef A2_GO_BACK_N_HPP
#define A2_GO_BACK_N_HPP

#include "protocol.hpp"
#include <string>
#include <vector>
#include <deque>

namespace net {

using std::string;
using std::vector;
using std::deque;

// packet tracking metadata
struct GbnPacketInfo {
    uint64_t abs_index;
    uint8_t  seq_num;
    Frame    frame;
    TimePoint send_time;
    bool     retransmitted;
};

// go-back-n sender
class GoBackNSender {
public:
    GoBackNSender(UdpSocket& socket, const Endpoint& receiver_ep,
                  const ProtocolConfig& config, const ChannelConfig& channel_cfg);

    bool send_file(const string& input_filepath);
    const ProtocolStats& get_stats() const { return stats_; }

    Frame Framing(uint8_t seq, const vector<uint8_t>& data);
    ChannelAction Channel(vector<uint8_t>& wire_bytes, const Frame& f);
    bool Send(const Frame& f);
    bool Timer(double timeout_ms);
    void Timeout(double sample_rtt);
    bool Recv(Frame& ack_frame, double timeout_ms);

private:
    UdpSocket& socket_;
    Endpoint receiver_ep_;
    ProtocolConfig config_;
    ChannelSimulator channel_;
    FrameTimer base_timer_;
    TimeoutCalculator timeout_calc_;
    ProtocolStats stats_;
    uint32_t seq_mod_;
};

// go-back-n receiver
class GoBackNReceiver {
public:
    GoBackNReceiver(UdpSocket& socket, const ProtocolConfig& config,
                    const ChannelConfig& ack_channel_cfg = ChannelConfig());

    bool receive_file(const string& output_filepath);
    const ProtocolStats& get_stats() const { return stats_; }

    bool Recv(Frame& frame, Endpoint& sender_ep, int timeout_ms = -1);
    bool Check(const Frame& frame);
    bool Send(const Frame& ack_frame, const Endpoint& sender_ep);

private:
    UdpSocket& socket_;
    ProtocolConfig config_;
    ChannelSimulator channel_;
    ProtocolStats stats_;
    uint32_t seq_mod_;
};

} // namespace net

#endif // A2_GO_BACK_N_HPP
