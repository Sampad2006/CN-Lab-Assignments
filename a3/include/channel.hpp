#ifndef A3_CHANNEL_HPP
#define A3_CHANNEL_HPP

#include "frame.hpp"
#include "socket.hpp"
#include <map>
#include <mutex>
#include <string>
#include <cstdint>

namespace csma {

struct ActiveTx {
    uint32_t station_id;
    uint32_t frame_seq;
    uint64_t start_time_us;
    uint32_t duration_ms;
    bool collided;
    Endpoint endpoint;
};

struct ChannelStats {
    uint64_t total_sense_requests{0};
    uint64_t total_tx_attempts{0};
    uint64_t successful_frames{0};
    uint64_t collided_frames{0};
    uint64_t collision_events{0};
    uint64_t jam_signals{0};
    uint64_t total_bytes_delivered{0};
    double total_busy_time_ms{0.0};
    double total_collision_time_ms{0.0};
    double total_sim_time_ms{0.0};
    double throughput_kbps{0.0};
    double channel_utilization{0.0};      // Useful airtime / Total time
    double transmission_efficiency{0.0};  // Successful frames / Total attempts
};

class Channel {
public:
    Channel();
    ~Channel() = default;

    ChannelState get_state();
    
    // Process sensing requests (models propagation delay tau)
    ChannelState handle_sense(uint32_t station_id);

    // Process start of transmission from a station
    bool handle_start_tx(uint32_t station_id, uint32_t frame_seq, uint32_t duration_ms, const Endpoint& ep);

    // Check collision status for a transmitting station (CSMA/CD)
    bool handle_check_cd(uint32_t station_id);

    // Process end of transmission
    bool handle_end_tx(uint32_t station_id, uint32_t frame_seq, uint32_t payload_len);

    // Process jam signal (CSMA/CD)
    void handle_jam(uint32_t station_id);

    // Channel statistics
    ChannelStats get_stats();
    void reset();

    void set_verbose(bool verbose) { verbose_ = verbose; }
    void set_prop_delay_ms(double prop_delay_ms) {
        std::lock_guard<std::mutex> lock(mtx_);
        prop_delay_us_ = static_cast<uint64_t>(prop_delay_ms * 1000.0);
    }

private:
    void update_state_locked();

    std::mutex mtx_;
    ChannelState state_{ChannelState::IDLE};
    std::map<uint32_t, ActiveTx> active_transmissions_;
    
    uint64_t start_sim_time_us_{0};
    uint64_t last_state_change_us_{0};
    uint64_t first_tx_start_us_{0};
    uint64_t prop_delay_us_{1000}; // 1.0 ms propagation delay / vulnerable period tau

    ChannelStats stats_;
    bool verbose_{true};
};

} // namespace csma

#endif // A3_CHANNEL_HPP
