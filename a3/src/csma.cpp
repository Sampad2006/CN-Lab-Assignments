#include "csma.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace csma {

CsmaStation::CsmaStation(const CsmaConfig& config)
    : config_(config),
      server_ep_("/tmp/csma_channel_" + std::to_string(config.server_port) + ".sock"),
      rng_(std::random_device{}() ^ (config.station_id * 7919 + 1337)) {
    stats_.station_id = config.station_id;
    stats_.protocol = config.protocol;
}

CsmaStation::~CsmaStation() {
    sock_.close();
}

bool CsmaStation::init() {
    if (!sock_.bind_auto(config_.server_port, config_.station_id)) {
        std::cerr << "[Station " << config_.station_id << "] Error binding socket\n";
        return false;
    }
    server_ep_ = Endpoint("/tmp/csma_channel_" + std::to_string(config_.server_port) + ".sock");
    return true;
}

ChannelState CsmaStation::sense_channel() {
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_SENSE_REQ);
    req.header.station_id = config_.station_id;
    req.header.timestamp_us = get_current_time_us();

    std::vector<uint8_t> out = serialize_message(req);
    sock_.send_to(out, server_ep_);

    std::vector<uint8_t> in(1024);
    Endpoint src;
    ssize_t n = sock_.recv_from(in.data(), in.size(), src, 50);
    if (n > 0) {
        Message resp;
        if (deserialize_message(in.data(), n, resp)) {
            return static_cast<ChannelState>(resp.header.channel_state);
        }
    }
    return ChannelState::IDLE;
}

bool CsmaStation::start_tx(uint32_t frame_seq, uint32_t duration_ms) {
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_START_TX);
    req.header.station_id = config_.station_id;
    req.header.frame_seq = frame_seq;
    req.header.duration_ms = duration_ms;
    req.header.timestamp_us = get_current_time_us();

    std::vector<uint8_t> out = serialize_message(req);
    sock_.send_to(out, server_ep_);

    std::vector<uint8_t> in(1024);
    Endpoint src;
    ssize_t n = sock_.recv_from(in.data(), in.size(), src, 50);
    if (n > 0) {
        Message resp;
        if (deserialize_message(in.data(), n, resp)) {
            return (resp.header.collided == 0);
        }
    }
    return true;
}

bool CsmaStation::check_collision_cd() {
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_CHECK_CD);
    req.header.station_id = config_.station_id;
    req.header.timestamp_us = get_current_time_us();

    std::vector<uint8_t> out = serialize_message(req);
    sock_.send_to(out, server_ep_);

    std::vector<uint8_t> in(1024);
    Endpoint src;
    ssize_t n = sock_.recv_from(in.data(), in.size(), src, 20);
    if (n > 0) {
        Message resp;
        if (deserialize_message(in.data(), n, resp)) {
            return (resp.header.collided != 0);
        }
    }
    return false;
}

bool CsmaStation::end_tx(uint32_t frame_seq, const std::string& payload) {
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_END_TX);
    req.header.station_id = config_.station_id;
    req.header.frame_seq = frame_seq;
    req.payload = payload;
    req.header.timestamp_us = get_current_time_us();

    std::vector<uint8_t> out = serialize_message(req);
    sock_.send_to(out, server_ep_);

    std::vector<uint8_t> in(1024);
    Endpoint src;
    ssize_t n = sock_.recv_from(in.data(), in.size(), src, 50);
    if (n > 0) {
        Message resp;
        if (deserialize_message(in.data(), n, resp)) {
            return (resp.header.collided == 0);
        }
    }
    return false;
}

void CsmaStation::send_jam() {
    Message req;
    req.header.msg_type = static_cast<uint8_t>(MsgType::MSG_JAM);
    req.header.station_id = config_.station_id;
    req.header.timestamp_us = get_current_time_us();

    std::vector<uint8_t> out = serialize_message(req);
    sock_.send_to(out, server_ep_);

    // Emit jam signal duration
    precise_sleep_ms(config_.jam_time_ms);
}

double CsmaStation::compute_beb_backoff(int retry_k) {
    int max_k = std::min(retry_k, config_.backoff_limit);
    int max_slots = (1 << max_k) - 1;
    if (max_slots < 1) max_slots = 1;
    std::uniform_int_distribution<int> dist(0, max_slots);
    int r = dist(rng_);
    return r * config_.slot_time_ms;
}

double CsmaStation::compute_uniform_backoff(int max_slots) {
    if (max_slots < 1) max_slots = 1;
    std::uniform_int_distribution<int> dist(1, max_slots);
    int r = dist(rng_);
    return r * config_.slot_time_ms;
}

// -------------------------------------------------------------
// 1. NON-PERSISTENT CSMA
// -------------------------------------------------------------
bool CsmaStation::run_non_persistent(uint32_t frame_seq, uint64_t start_us) {
    int retry = 0;
    std::string payload(config_.payload_size, 'A' + (config_.station_id % 26));

    while (retry < config_.max_retries) {
        ChannelState st = sense_channel();

        if (st == ChannelState::IDLE) {
            // Channel is IDLE -> transmit immediately
            start_tx(frame_seq, static_cast<uint32_t>(config_.tx_time_ms));
            precise_sleep_ms(config_.tx_time_ms);

            bool success = end_tx(frame_seq, payload);
            if (success) {
                uint64_t done_us = get_current_time_us();
                double delay_ms = (done_us - start_us) / 1000.0;
                stats_.per_packet_delays.push_back(delay_ms);
                stats_.total_delay_ms += delay_ms;
                stats_.frames_successful++;
                if (config_.verbose) {
                    std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                              << " delivered successfully! (Delay: " << std::fixed << std::setprecision(2) 
                              << delay_ms << " ms)\n";
                }
                return true;
            } else {
                // Collided during transmission
                stats_.total_collisions++;
                retry++;
                double backoff = compute_beb_backoff(retry);
                stats_.total_backoff_ms += backoff;
                if (config_.verbose) {
                    std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                              << " collided during transmission. Backoff " << backoff << " ms (Attempt " << retry << ")\n";
                }
                precise_sleep_ms(backoff);
            }
        } else {
            // Channel is BUSY -> Non-persistent deferral (backoff and sense again)
            double backoff = compute_uniform_backoff(8);
            stats_.total_backoff_ms += backoff;
            precise_sleep_ms(backoff);
        }
    }

    stats_.frames_dropped++;
    if (config_.verbose) {
        std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                  << " DROPPED after " << config_.max_retries << " attempts\n";
    }
    return false;
}

// -------------------------------------------------------------
// 2. 1-PERSISTENT CSMA
// -------------------------------------------------------------
bool CsmaStation::run_one_persistent(uint32_t frame_seq, uint64_t start_us) {
    int retry = 0;
    std::string payload(config_.payload_size, 'A' + (config_.station_id % 26));

    while (retry < config_.max_retries) {
        // Continuous carrier sensing until IDLE
        while (sense_channel() != ChannelState::IDLE) {
            precise_sleep_ms(std::max(0.05, config_.slot_time_ms / 10.0));
        }

        // Channel became IDLE -> Transmit immediately with probability 1.0
        start_tx(frame_seq, static_cast<uint32_t>(config_.tx_time_ms));
        precise_sleep_ms(config_.tx_time_ms);

        bool success = end_tx(frame_seq, payload);
        if (success) {
            uint64_t done_us = get_current_time_us();
            double delay_ms = (done_us - start_us) / 1000.0;
            stats_.per_packet_delays.push_back(delay_ms);
            stats_.total_delay_ms += delay_ms;
            stats_.frames_successful++;
            if (config_.verbose) {
                std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                          << " delivered successfully! (Delay: " << std::fixed << std::setprecision(2) 
                          << delay_ms << " ms)\n";
            }
            return true;
        } else {
            // Collision occurred
            stats_.total_collisions++;
            retry++;
            double backoff = compute_beb_backoff(retry);
            stats_.total_backoff_ms += backoff;
            if (config_.verbose) {
                std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                          << " collided. Backoff " << backoff << " ms (Attempt " << retry << ")\n";
            }
            precise_sleep_ms(backoff);
        }
    }

    stats_.frames_dropped++;
    return false;
}

// -------------------------------------------------------------
// 3. p-PERSISTENT CSMA
// -------------------------------------------------------------
bool CsmaStation::run_p_persistent(uint32_t frame_seq, uint64_t start_us) {
    int retry = 0;
    std::string payload(config_.payload_size, 'A' + (config_.station_id % 26));

    while (retry < config_.max_retries) {
        // Wait until channel is IDLE
        while (sense_channel() != ChannelState::IDLE) {
            precise_sleep_ms(config_.slot_time_ms);
        }

        // Slotted probability decision loop
        bool transmitted = false;
        while (!transmitted && retry < config_.max_retries) {
            double roll = dist_p_(rng_);

            if (roll <= config_.p_value) {
                // Transmit in current slot
                transmitted = true;
                start_tx(frame_seq, static_cast<uint32_t>(config_.tx_time_ms));
                precise_sleep_ms(config_.tx_time_ms);

                bool success = end_tx(frame_seq, payload);
                if (success) {
                    uint64_t done_us = get_current_time_us();
                    double delay_ms = (done_us - start_us) / 1000.0;
                    stats_.per_packet_delays.push_back(delay_ms);
                    stats_.total_delay_ms += delay_ms;
                    stats_.frames_successful++;
                    if (config_.verbose) {
                        std::cout << "[Station " << config_.station_id << "] Frame " << frame_seq 
                                  << " delivered successfully (p=" << config_.p_value << ")! (Delay: " 
                                  << std::fixed << std::setprecision(2) << delay_ms << " ms)\n";
                    }
                    return true;
                } else {
                    stats_.total_collisions++;
                    retry++;
                    double backoff = compute_beb_backoff(retry);
                    stats_.total_backoff_ms += backoff;
                    precise_sleep_ms(backoff);
                    break; // Restart sensing
                }
            } else {
                // Defer for 1 slot time
                stats_.total_backoff_ms += config_.slot_time_ms;
                precise_sleep_ms(config_.slot_time_ms);

                // Sense again in the new slot
                if (sense_channel() != ChannelState::IDLE) {
                    // Channel was seized by another station; wait until next idle cycle
                    break;
                }
            }
        }
    }

    stats_.frames_dropped++;
    return false;
}

// -------------------------------------------------------------
// 4. CSMA/CD (Collision Detection & Jam Signal)
// -------------------------------------------------------------
bool CsmaStation::run_csma_cd(uint32_t frame_seq, uint64_t start_us) {
    int retry = 0;
    std::string payload(config_.payload_size, 'A' + (config_.station_id % 26));

    while (retry < config_.max_retries) {
        // 1-persistent carrier sense
        while (sense_channel() != ChannelState::IDLE) {
            precise_sleep_ms(std::max(0.05, config_.slot_time_ms / 10.0));
        }

        // Start transmission
        start_tx(frame_seq, static_cast<uint32_t>(config_.tx_time_ms));

        // Listen-while-talk collision detection during transmission duration
        double elapsed_tx_ms = 0.0;
        double check_step_ms = std::max(0.1, config_.slot_time_ms / 5.0);
        bool collision_detected = false;

        while (elapsed_tx_ms < config_.tx_time_ms) {
            precise_sleep_ms(check_step_ms);
            elapsed_tx_ms += check_step_ms;

            if (check_collision_cd()) {
                collision_detected = true;
                break;
            }
        }

        if (collision_detected) {
            // IMMEDIATELY ABORT TRANSMISSION & SEND JAM SIGNAL
            send_jam();
            stats_.total_collisions++;
            stats_.total_jams++;
            retry++;

            // Truncated Binary Exponential Backoff (BEB)
            double backoff = compute_beb_backoff(retry);
            stats_.total_backoff_ms += backoff;

            if (config_.verbose) {
                std::cout << "[Station " << config_.station_id << "] CSMA/CD: Collision detected at " 
                          << elapsed_tx_ms << " ms! Aborted, Jammed, Backoff " << backoff 
                          << " ms (Attempt " << retry << ")\n";
            }
            precise_sleep_ms(backoff);
        } else {
            // Full transmission completed without collision
            bool success = end_tx(frame_seq, payload);
            if (success) {
                uint64_t done_us = get_current_time_us();
                double delay_ms = (done_us - start_us) / 1000.0;
                stats_.per_packet_delays.push_back(delay_ms);
                stats_.total_delay_ms += delay_ms;
                stats_.frames_successful++;
                if (config_.verbose) {
                    std::cout << "[Station " << config_.station_id << "] CSMA/CD: Frame " << frame_seq 
                              << " delivered successfully! (Delay: " << std::fixed << std::setprecision(2) 
                              << delay_ms << " ms)\n";
                }
                return true;
            } else {
                // Collided right at the boundary
                stats_.total_collisions++;
                retry++;
                double backoff = compute_beb_backoff(retry);
                stats_.total_backoff_ms += backoff;
                precise_sleep_ms(backoff);
            }
        }
    }

    stats_.frames_dropped++;
    return false;
}

bool CsmaStation::transmit_frame(uint32_t frame_seq) {
    uint64_t start_us = get_current_time_us();
    stats_.frames_attempted++;

    switch (config_.protocol) {
        case ProtocolType::NON_PERSISTENT:
            return run_non_persistent(frame_seq, start_us);
        case ProtocolType::ONE_PERSISTENT:
            return run_one_persistent(frame_seq, start_us);
        case ProtocolType::P_PERSISTENT:
            return run_p_persistent(frame_seq, start_us);
        case ProtocolType::CSMA_CD:
            return run_csma_cd(frame_seq, start_us);
        default:
            return run_non_persistent(frame_seq, start_us);
    }
}

void CsmaStation::run() {
    uint64_t run_start_us = get_current_time_us();

    for (int i = 1; i <= config_.total_frames; ++i) {
        transmit_frame(i);
        if (config_.packet_interval_ms > 0) {
            precise_sleep_ms(config_.packet_interval_ms);
        }
    }

    uint64_t run_end_us = get_current_time_us();
    stats_.total_sim_time_ms = (run_end_us - run_start_us) / 1000.0;

    if (!stats_.per_packet_delays.empty()) {
        double sum = 0.0;
        stats_.min_delay_ms = stats_.per_packet_delays[0];
        stats_.max_delay_ms = stats_.per_packet_delays[0];
        for (double d : stats_.per_packet_delays) {
            sum += d;
            if (d < stats_.min_delay_ms) stats_.min_delay_ms = d;
            if (d > stats_.max_delay_ms) stats_.max_delay_ms = d;
        }
        stats_.avg_delay_ms = sum / stats_.per_packet_delays.size();
    }

    if (stats_.total_sim_time_ms > 0) {
        uint64_t total_payload_bytes = stats_.frames_successful * config_.payload_size;
        stats_.throughput_kbps = (total_payload_bytes * 8.0) / stats_.total_sim_time_ms;
    }
}

void CsmaStation::print_summary() const {
    std::cout << "\n--------------------------------------------------------\n"
              << " Station " << config_.station_id << " Summary (" << protocol_type_to_str(config_.protocol) << ")\n"
              << "--------------------------------------------------------\n"
              << "Frames Attempted    : " << stats_.frames_attempted << "\n"
              << "Frames Successful   : " << stats_.frames_successful << " (" 
              << (stats_.frames_attempted > 0 ? (stats_.frames_successful * 100.0 / stats_.frames_attempted) : 0.0) << "%)\n"
              << "Frames Dropped      : " << stats_.frames_dropped << "\n"
              << "Collisions Recorded : " << stats_.total_collisions << "\n"
              << "Jams Emitted (CD)   : " << stats_.total_jams << "\n"
              << "Average Delay       : " << std::fixed << std::setprecision(2) << stats_.avg_delay_ms << " ms\n"
              << "Min / Max Delay     : " << stats_.min_delay_ms << " ms / " << stats_.max_delay_ms << " ms\n"
              << "Total Backoff Time  : " << stats_.total_backoff_ms << " ms\n"
              << "Throughput          : " << stats_.throughput_kbps << " kbps\n"
              << "--------------------------------------------------------\n";
}

} // namespace csma
