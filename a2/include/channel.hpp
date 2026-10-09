#ifndef A2_CHANNEL_HPP
#define A2_CHANNEL_HPP

#include <vector>
#include <cstdint>
#include <random>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "frame.hpp"
#include "crc.hpp"
#include "socket.hpp"

namespace net {

using std::vector;

enum class ChannelAction {
    PASS,
    CORRUPT,
    DROP
};

// simulated channel config
struct ChannelConfig {
    double loss_prob;
    double error_prob;
    double min_delay_ms;
    double max_delay_ms;

    ChannelConfig(double loss = 0.0, double error = 0.0,
                  double min_d = 0.0, double max_d = 0.0)
        : loss_prob(loss), error_prob(error),
          min_delay_ms(min_d), max_delay_ms(max_d) {}
};

// link channel simulator
class ChannelSimulator {
public:
    ChannelSimulator(const ChannelConfig& config = ChannelConfig());
    ~ChannelSimulator();

    void set_config(const ChannelConfig& config);
    const ChannelConfig& get_config() const { return config_; }

    ChannelAction process_outgoing(vector<uint8_t>& wire_bytes, const Frame& frame_info);
    ChannelAction transmit(UdpSocket& socket, const Endpoint& dest,
                           vector<uint8_t>& wire_bytes, const Frame& frame_info);
    void flush();

    uint64_t get_total_frames() const { return total_frames_; }
    uint64_t get_dropped_frames() const { return dropped_frames_; }
    uint64_t get_corrupted_frames() const { return corrupted_frames_; }
    uint64_t get_passed_frames() const { return passed_frames_; }
    void reset_stats();

private:
    struct InFlightPacket {
        std::chrono::steady_clock::time_point delivery_time;
        UdpSocket* socket;
        Endpoint dest;
        vector<uint8_t> wire_bytes;
    };

    void worker_loop();

    ChannelConfig config_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_real_;

    uint64_t total_frames_;
    uint64_t dropped_frames_;
    uint64_t corrupted_frames_;
    uint64_t passed_frames_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::condition_variable cv_empty_;
    std::queue<InFlightPacket> pipe_;
    std::chrono::steady_clock::time_point last_delivery_time_;
    bool stop_worker_;
    std::thread worker_;
};

} // namespace net

#endif // A2_CHANNEL_HPP
