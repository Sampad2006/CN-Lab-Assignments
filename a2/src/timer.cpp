#include "timer.hpp"
#include <cmath>
#include <algorithm>

using namespace std;

namespace net {

FrameTimer::FrameTimer() : running_(false) {}

void FrameTimer::start() {
    start_time_ = Clock::now();
    running_ = true;
}

void FrameTimer::stop() {
    running_ = false;
}

void FrameTimer::reset() {
    start_time_ = Clock::now();
    running_ = true;
}

double FrameTimer::elapsed_ms() const {
    if (!running_) return 0.0;
    auto now = Clock::now();
    chrono::duration<double, milli> diff = now - start_time_;
    return diff.count();
}

bool FrameTimer::is_expired(double timeout_ms) const {
    if (!running_) return false;
    return elapsed_ms() >= timeout_ms;
}

TimeoutCalculator::TimeoutCalculator(double initial_rto_ms, double min_rto_ms, double max_rto_ms)
    : srtt_ms_(initial_rto_ms),
      rttvar_ms_(initial_rto_ms / 2.0),
      rto_ms_(initial_rto_ms),
      min_rto_ms_(min_rto_ms),
      max_rto_ms_(max_rto_ms),
      initialized_(false) {}

// compute rto using jacobson's algorithm
void TimeoutCalculator::update_rtt(double sample_rtt_ms) {
    if (!initialized_) {
        srtt_ms_ = sample_rtt_ms;
        rttvar_ms_ = sample_rtt_ms / 2.0;
        initialized_ = true;
    } else {
        rttvar_ms_ = (1.0 - beta_) * rttvar_ms_ + beta_ * fabs(srtt_ms_ - sample_rtt_ms);
        srtt_ms_ = (1.0 - alpha_) * srtt_ms_ + alpha_ * sample_rtt_ms;
    }

    rto_ms_ = srtt_ms_ + 4.0 * rttvar_ms_;
    rto_ms_ = clamp(rto_ms_, min_rto_ms_, max_rto_ms_);
}

} // namespace net
