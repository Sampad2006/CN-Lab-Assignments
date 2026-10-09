#ifndef A2_STOP_AND_WAIT_HPP
#define A2_STOP_AND_WAIT_HPP

#include "protocol.hpp"
#include <string>
#include <vector>

namespace net {

using std::string;
using std::vector;

class StopAndWaitSender {
public:
    StopAndWaitSender(UdpSocket& socket, const Endpoint& receiver_ep,
                      const ProtocolConfig& config, const ChannelConfig& channel_cfg);

    bool send_file(const string& input_filepath);
    const ProtocolStats& get_stats() const { return stats_; }

    // flow control operations
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
    FrameTimer timer_;
    TimeoutCalculator timeout_calc_;
    ProtocolStats stats_;
};

class StopAndWaitReceiver {
public:
    StopAndWaitReceiver(UdpSocket& socket, const ProtocolConfig& config,
                        const ChannelConfig& ack_channel_cfg = ChannelConfig());

    bool receive_file(const string& output_filepath);
    const ProtocolStats& get_stats() const { return stats_; }

    // receiver operations
    bool Recv(Frame& frame, Endpoint& sender_ep, int timeout_ms = -1);
    bool Check(const Frame& frame);
    bool Send(const Frame& ack_frame, const Endpoint& sender_ep);

private:
    UdpSocket& socket_;
    ProtocolConfig config_;
    ChannelSimulator channel_;
    ProtocolStats stats_;
};

} // namespace net

#endif // A2_STOP_AND_WAIT_HPP
