#ifndef A3_CSMA_HPP
#define A3_CSMA_HPP

#include "frame.hpp"
#include "socket.hpp"
#include <string>
#include <vector>
#include <random>

namespace csma {

struct CsmaConfig {
    ProtocolType protocol{ProtocolType::NON_PERSISTENT};
    uint32_t station_id{1};
    double p_value{0.1};              // For p-persistent CSMA (0.0 < p <= 1.0)
    double slot_time_ms{1.0};         // Base slot duration tau in ms
    double tx_time_ms{5.0};           // Frame transmission time in ms
    double jam_time_ms{1.0};          // Jamming signal duration for CSMA/CD
    int max_retries{16};              // Maximum retry attempts before packet drop
    int backoff_limit{10};            // Maximum exponent for BEB (2^10 = 1024 slots)
    int total_frames{10};             // Number of frames to transmit
    int payload_size{64};             // Frame payload size in bytes
    double packet_interval_ms{0.0};   // Optional inter-arrival delay between packets
    std::string server_ip{"127.0.0.1"};
    uint16_t server_port{9999};
    bool verbose{true};
};

struct StationStats {
    uint32_t station_id{0};
    ProtocolType protocol{ProtocolType::NON_PERSISTENT};
    int frames_attempted{0};
    int frames_successful{0};
    int frames_dropped{0};
    int total_collisions{0};
    int total_jams{0};
    double total_backoff_ms{0.0};
    double total_delay_ms{0.0};
    double avg_delay_ms{0.0};
    double min_delay_ms{0.0};
    double max_delay_ms{0.0};
    double total_sim_time_ms{0.0};
    double throughput_kbps{0.0};
    std::vector<double> per_packet_delays;
};

class CsmaStation {
public:
    explicit CsmaStation(const CsmaConfig& config);
    ~CsmaStation();

    bool init();
    void run();

    StationStats get_stats() const { return stats_; }
    void print_summary() const;

private:
    // Core transmission dispatcher
    bool transmit_frame(uint32_t frame_seq);

    // Protocol variants
    bool run_non_persistent(uint32_t frame_seq, uint64_t start_us);
    bool run_one_persistent(uint32_t frame_seq, uint64_t start_us);
    bool run_p_persistent(uint32_t frame_seq, uint64_t start_us);
    bool run_csma_cd(uint32_t frame_seq, uint64_t start_us);

    // Channel communication helpers
    ChannelState sense_channel();
    bool start_tx(uint32_t frame_seq, uint32_t duration_ms);
    bool check_collision_cd();
    bool end_tx(uint32_t frame_seq, const std::string& payload);
    void send_jam();

    // Backoff algorithms
    double compute_beb_backoff(int retry_k);
    double compute_uniform_backoff(int max_slots);

    CsmaConfig config_;
    StationStats stats_;
    UdpSocket sock_;
    Endpoint server_ep_;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_p_{0.0, 1.0};
};

} // namespace csma

#endif // A3_CSMA_HPP
