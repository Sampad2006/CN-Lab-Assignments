#include "channel.hpp"
#include <iostream>
#include <iomanip>

namespace csma {

Channel::Channel() {
    reset();
}

void Channel::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    state_ = ChannelState::IDLE;
    active_transmissions_.clear();
    stats_ = ChannelStats();
    start_sim_time_us_ = get_current_time_us();
    last_state_change_us_ = start_sim_time_us_;
    first_tx_start_us_ = 0;
}

ChannelState Channel::get_state() {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

ChannelState Channel::handle_sense(uint32_t station_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    stats_.total_sense_requests++;

    if (active_transmissions_.empty()) {
        return ChannelState::IDLE;
    }

    uint64_t now_us = get_current_time_us();

    // If the station calling handle_sense is already transmitting, return current channel state
    if (active_transmissions_.find(station_id) != active_transmissions_.end()) {
        return state_;
    }

    // Vulnerable Propagation Delay Window (tau):
    // If the transmission epoch began less than prop_delay_us_ ago, the electromagnetic wave
    // has not finished propagating to this station yet. Thus, this station senses IDLE!
    if (first_tx_start_us_ > 0 && (now_us - first_tx_start_us_ < prop_delay_us_)) {
        return ChannelState::IDLE;
    }

    return state_;
}

bool Channel::handle_start_tx(uint32_t station_id, uint32_t frame_seq, uint32_t duration_ms, const Endpoint& ep) {
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t now_us = get_current_time_us();

    if (start_sim_time_us_ == 0) {
        start_sim_time_us_ = now_us;
    }

    stats_.total_tx_attempts++;

    ActiveTx tx;
    tx.station_id = station_id;
    tx.frame_seq = frame_seq;
    tx.start_time_us = now_us;
    tx.duration_ms = duration_ms;
    tx.endpoint = ep;
    tx.collided = false;

    if (active_transmissions_.empty()) {
        // Channel was IDLE -> now BUSY
        state_ = ChannelState::BUSY;
        first_tx_start_us_ = now_us;
        active_transmissions_[station_id] = tx;
        if (verbose_) {
            std::cout << "[Channel] Station " << station_id << " STARTED TX of Frame " << frame_seq 
                      << " (Channel -> BUSY)\n";
        }
        return true;
    } else {
        // Channel was already BUSY or in COLLISION -> A COLLISION OCCURS!
        bool first_collision_event = (state_ != ChannelState::COLLISION);
        state_ = ChannelState::COLLISION;
        
        // Mark all existing transmissions as collided
        for (auto& pair : active_transmissions_) {
            pair.second.collided = true;
        }

        tx.collided = true;
        active_transmissions_[station_id] = tx;

        if (first_collision_event) {
            stats_.collision_events++;
        }

        if (verbose_) {
            std::cout << "[Channel] *** COLLISION DETECTED *** Station " << station_id 
                      << " started TX while channel was occupied! Total contending: " 
                      << active_transmissions_.size() << "\n";
        }
        return false;
    }
}

bool Channel::handle_check_cd(uint32_t station_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = active_transmissions_.find(station_id);
    if (it == active_transmissions_.end()) {
        return (state_ == ChannelState::COLLISION);
    }
    return it->second.collided || (state_ == ChannelState::COLLISION);
}

bool Channel::handle_end_tx(uint32_t station_id, uint32_t frame_seq, uint32_t payload_len) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = active_transmissions_.find(station_id);
    if (it == active_transmissions_.end()) {
        update_state_locked();
        return false;
    }

    bool was_collided = it->second.collided;
    double tx_duration_ms = it->second.duration_ms;

    active_transmissions_.erase(it);
    update_state_locked();

    if (!was_collided) {
        stats_.successful_frames++;
        stats_.total_bytes_delivered += payload_len;
        stats_.total_busy_time_ms += tx_duration_ms;
        if (verbose_) {
            std::cout << "[Channel] Station " << station_id << " COMPLETED TX of Frame " << frame_seq 
                      << " [SUCCESS] (" << payload_len << " bytes)\n";
        }
        return true;
    } else {
        stats_.collided_frames++;
        stats_.total_collision_time_ms += tx_duration_ms;
        if (verbose_) {
            std::cout << "[Channel] Station " << station_id << " ENDED TX of Frame " << frame_seq 
                      << " [COLLIDED / CORRUPTED]\n";
        }
        return false;
    }
}

void Channel::handle_jam(uint32_t station_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    stats_.jam_signals++;
    state_ = ChannelState::COLLISION;

    auto it = active_transmissions_.find(station_id);
    if (it != active_transmissions_.end()) {
        it->second.collided = true;
    }
    for (auto& pair : active_transmissions_) {
        pair.second.collided = true;
    }

    if (verbose_) {
        std::cout << "[Channel] Station " << station_id << " EMITTED JAM SIGNAL (Aborting TX)\n";
    }
}

void Channel::update_state_locked() {
    if (active_transmissions_.empty()) {
        state_ = ChannelState::IDLE;
        first_tx_start_us_ = 0;
    } else if (active_transmissions_.size() == 1) {
        if (active_transmissions_.begin()->second.collided) {
            state_ = ChannelState::COLLISION;
        } else {
            state_ = ChannelState::BUSY;
        }
    } else {
        state_ = ChannelState::COLLISION;
    }
}

ChannelStats Channel::get_stats() {
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t now_us = get_current_time_us();
    double total_sim_ms = (now_us > start_sim_time_us_) ? (now_us - start_sim_time_us_) / 1000.0 : 0.001;
    stats_.total_sim_time_ms = total_sim_ms;

    if (total_sim_ms > 0) {
        stats_.throughput_kbps = (stats_.total_bytes_delivered * 8.0) / total_sim_ms;
        stats_.channel_utilization = (stats_.total_busy_time_ms / total_sim_ms) * 100.0;
        if (stats_.channel_utilization > 100.0) stats_.channel_utilization = 100.0;
    }

    if (stats_.total_tx_attempts > 0) {
        stats_.transmission_efficiency = (static_cast<double>(stats_.successful_frames) / stats_.total_tx_attempts) * 100.0;
    } else {
        stats_.transmission_efficiency = 0.0;
    }

    return stats_;
}

} // namespace csma
