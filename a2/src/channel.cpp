#include "channel.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <algorithm>

using namespace std;

namespace net {

ChannelSimulator::ChannelSimulator(const ChannelConfig& config)
    : config_(config),
      rng_(random_device{}()),
      dist_real_(0.0, 1.0),
      total_frames_(0),
      dropped_frames_(0),
      corrupted_frames_(0),
      passed_frames_(0),
      last_delivery_time_(chrono::steady_clock::now()),
      stop_worker_(false) {
    worker_ = thread(&ChannelSimulator::worker_loop, this);
}

ChannelSimulator::~ChannelSimulator() {
    flush();
    {
        lock_guard<mutex> lock(mtx_);
        stop_worker_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ChannelSimulator::set_config(const ChannelConfig& config) {
    config_ = config;
}

void ChannelSimulator::reset_stats() {
    total_frames_ = 0;
    dropped_frames_ = 0;
    corrupted_frames_ = 0;
    passed_frames_ = 0;
}

void ChannelSimulator::flush() {
    unique_lock<mutex> lock(mtx_);
    cv_empty_.wait(lock, [this]() { return pipe_.empty(); });
}

void ChannelSimulator::worker_loop() {
    while (true) {
        InFlightPacket pkt;
        {
            unique_lock<mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return stop_worker_ || !pipe_.empty(); });
            if (stop_worker_ && pipe_.empty()) {
                break;
            }
            pkt = pipe_.front();
        }

        auto now = chrono::steady_clock::now();
        if (pkt.delivery_time > now) {
            this_thread::sleep_until(pkt.delivery_time);
        }

        if (pkt.socket) {
            pkt.socket->send_to(pkt.wire_bytes, pkt.dest);
        }

        {
            lock_guard<mutex> lock(mtx_);
            if (!pipe_.empty()) {
                pipe_.pop();
            }
            if (pipe_.empty()) {
                cv_empty_.notify_all();
            }
        }
    }
}

ChannelAction ChannelSimulator::process_outgoing(vector<uint8_t>& wire_bytes, const Frame& frame_info) {
    total_frames_++;

    // random propagation delay
    if (config_.max_delay_ms > 0.0) {
        double delay_ms = config_.min_delay_ms;
        if (config_.max_delay_ms > config_.min_delay_ms) {
            uniform_real_distribution<double> delay_dist(config_.min_delay_ms, config_.max_delay_ms);
            delay_ms = delay_dist(rng_);
        }
        if (delay_ms > 0.0) {
            this_thread::sleep_for(chrono::duration<double, milli>(delay_ms));
        }
    }

    // simulate packet loss
    if (config_.loss_prob > 0.0 && dist_real_(rng_) < config_.loss_prob) {
        dropped_frames_++;
        cout << "  [CHANNEL SIM] DROPPED: " << frame_info.to_string()
             << " (simulated loss, prob=" << config_.loss_prob << ")\n";
        return ChannelAction::DROP;
    }

    // simulate bit corruption
    if (config_.error_prob > 0.0 && dist_real_(rng_) < config_.error_prob) {
        corrupted_frames_++;
        if (!wire_bytes.empty()) {
            inject_single_bit_error(wire_bytes.data(), wire_bytes.size());
        }
        cout << "  [CHANNEL SIM] CORRUPTED: " << frame_info.to_string()
             << " (simulated bit error, prob=" << config_.error_prob << ")\n";
        return ChannelAction::CORRUPT;
    }

    passed_frames_++;
    return ChannelAction::PASS;
}

ChannelAction ChannelSimulator::transmit(UdpSocket& socket, const Endpoint& dest,
                                         vector<uint8_t>& wire_bytes, const Frame& frame_info) {
    total_frames_++;

    // serialization delay (T_tx = 0.8 ms when delay > 0)
    if (config_.max_delay_ms > 0.0) {
        this_thread::sleep_for(chrono::microseconds(800));
    }

    // simulate packet loss
    if (config_.loss_prob > 0.0 && dist_real_(rng_) < config_.loss_prob) {
        dropped_frames_++;
        cout << "  [CHANNEL SIM] DROPPED: " << frame_info.to_string()
             << " (simulated loss, prob=" << config_.loss_prob << ")\n";
        return ChannelAction::DROP;
    }

    // simulate bit corruption
    ChannelAction action = ChannelAction::PASS;
    if (config_.error_prob > 0.0 && dist_real_(rng_) < config_.error_prob) {
        corrupted_frames_++;
        if (!wire_bytes.empty()) {
            inject_single_bit_error(wire_bytes.data(), wire_bytes.size());
        }
        cout << "  [CHANNEL SIM] CORRUPTED: " << frame_info.to_string()
             << " (simulated bit error, prob=" << config_.error_prob << ")\n";
        action = ChannelAction::CORRUPT;
    } else {
        passed_frames_++;
    }

    // async propagation delay across link
    if (config_.max_delay_ms > 0.0) {
        double delay_ms = config_.min_delay_ms;
        if (config_.max_delay_ms > config_.min_delay_ms) {
            uniform_real_distribution<double> delay_dist(config_.min_delay_ms, config_.max_delay_ms);
            delay_ms = delay_dist(rng_);
        }

        auto now = chrono::steady_clock::now();
        auto target_time = now + chrono::duration_cast<chrono::steady_clock::duration>(
                                     chrono::duration<double, milli>(delay_ms));

        {
            lock_guard<mutex> lock(mtx_);
            auto min_ordered_time = last_delivery_time_ + chrono::microseconds(400);
            if (target_time < min_ordered_time) {
                target_time = min_ordered_time;
            }
            last_delivery_time_ = target_time;
            pipe_.push({target_time, &socket, dest, wire_bytes});
        }
        cv_.notify_one();
    } else {
        socket.send_to(wire_bytes, dest);
    }

    return action;
}

} // namespace net
