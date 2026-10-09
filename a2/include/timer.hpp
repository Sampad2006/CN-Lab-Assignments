#ifndef A2_TIMER_HPP
#define A2_TIMER_HPP

#include <chrono>
#include <cstdint>

namespace net {

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

class FrameTimer {
public:
    FrameTimer();

    void start();
    void stop();
    void reset();

    bool is_running() const { return running_; }
    double elapsed_ms() const;
    bool is_expired(double timeout_ms) const;
    TimePoint get_start_time() const { return start_time_; }

private:
    TimePoint start_time_;
    bool running_;
};

// adaptive timeout calculator using jacobson's algorithm (rfc 6298)
class TimeoutCalculator {
public:
    TimeoutCalculator(double initial_rto_ms = 200.0,
                      double min_rto_ms = 50.0,
                      double max_rto_ms = 3000.0);

    void update_rtt(double sample_rtt_ms);

    double get_rto_ms() const { return rto_ms_; }
    double get_srtt_ms() const { return srtt_ms_; }
    double get_rttvar_ms() const { return rttvar_ms_; }

private:
    double srtt_ms_;
    double rttvar_ms_;
    double rto_ms_;
    double min_rto_ms_;
    double max_rto_ms_;
    bool initialized_;

    const double alpha_ = 0.125;
    const double beta_  = 0.25;
};

} // namespace net

#endif // A2_TIMER_HPP
