#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>

#include "frame.hpp"
#include "channel.hpp"
#include "csma.hpp"

using namespace csma;

struct SimConfig {
    ProtocolType protocol{ProtocolType::NON_PERSISTENT};
    int num_stations{5};
    double p_value{0.1};
    int frames_per_station{20};
    int payload_size{64};
    double slot_time_ms{1.0};
    double tx_time_ms{5.0};
    double jam_time_ms{1.0};
    int max_retries{16};
    int backoff_limit{10};
    bool csv_mode{false};
    bool verbose{false};
};

struct SimStationResult {
    uint32_t station_id{0};
    int frames_attempted{0};
    int frames_successful{0};
    int frames_dropped{0};
    int total_collisions{0};
    int total_jams{0};
    double total_delay_ms{0.0};
    std::vector<double> delays;
};

void run_station_sim(int station_id, const SimConfig& cfg, Channel& channel, 
                     SimStationResult& result, std::atomic<bool>& start_flag) {
    result.station_id = station_id;
    std::mt19937 rng(std::random_device{}() ^ (station_id * 7919 + 42));
    std::uniform_real_distribution<double> dist_p(0.0, 1.0);

    Endpoint ep("/tmp/sim_station_" + std::to_string(station_id));

    auto compute_beb = [&](int retry) -> double {
        int max_k = std::min(retry, cfg.backoff_limit);
        int max_slots = (1 << max_k) - 1;
        if (max_slots < 1) max_slots = 1;
        std::uniform_int_distribution<int> dist(0, max_slots);
        return dist(rng) * cfg.slot_time_ms;
    };

    auto compute_uniform = [&](int max_slots) -> double {
        if (max_slots < 1) max_slots = 1;
        std::uniform_int_distribution<int> dist(1, max_slots);
        return dist(rng) * cfg.slot_time_ms;
    };

    // Wait for all threads to be ready
    while (!start_flag.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    for (int frame_seq = 1; frame_seq <= cfg.frames_per_station; ++frame_seq) {
        uint64_t start_us = get_current_time_us();
        result.frames_attempted++;
        int retry = 0;
        bool delivered = false;

        while (retry < cfg.max_retries && !delivered) {
            if (cfg.protocol == ProtocolType::NON_PERSISTENT) {
                // Non-persistent CSMA
                ChannelState st = channel.handle_sense(station_id);
                if (st == ChannelState::IDLE) {
                    channel.handle_start_tx(station_id, frame_seq, static_cast<uint32_t>(cfg.tx_time_ms), ep);
                    precise_sleep_ms(cfg.tx_time_ms);

                    bool success = channel.handle_end_tx(station_id, frame_seq, cfg.payload_size);
                    if (success) {
                        delivered = true;
                        result.frames_successful++;
                        double delay = (get_current_time_us() - start_us) / 1000.0;
                        result.delays.push_back(delay);
                        result.total_delay_ms += delay;
                    } else {
                        result.total_collisions++;
                        retry++;
                        double backoff = compute_beb(retry);
                        precise_sleep_ms(backoff);
                    }
                } else {
                    // Channel BUSY -> back off immediately without continuous listening
                    double backoff = compute_uniform(10);
                    precise_sleep_ms(backoff);
                }
            } else if (cfg.protocol == ProtocolType::ONE_PERSISTENT) {
                // 1-persistent CSMA: Sense continuously until IDLE
                while (channel.handle_sense(station_id) != ChannelState::IDLE) {
                    precise_sleep_ms(std::max(0.05, cfg.slot_time_ms / 10.0));
                }

                // Transmit immediately upon idle (p = 1.0)
                channel.handle_start_tx(station_id, frame_seq, static_cast<uint32_t>(cfg.tx_time_ms), ep);
                precise_sleep_ms(cfg.tx_time_ms);

                bool success = channel.handle_end_tx(station_id, frame_seq, cfg.payload_size);
                if (success) {
                    delivered = true;
                    result.frames_successful++;
                    double delay = (get_current_time_us() - start_us) / 1000.0;
                    result.delays.push_back(delay);
                    result.total_delay_ms += delay;
                } else {
                    result.total_collisions++;
                    retry++;
                    double backoff = compute_beb(retry);
                    precise_sleep_ms(backoff);
                }
            } else if (cfg.protocol == ProtocolType::P_PERSISTENT) {
                // p-persistent CSMA: Wait until IDLE, then slot-by-slot decision
                while (channel.handle_sense(station_id) != ChannelState::IDLE) {
                    precise_sleep_ms(cfg.slot_time_ms);
                }

                bool attempted_in_p = false;
                while (!attempted_in_p && retry < cfg.max_retries) {
                    double roll = dist_p(rng);
                    if (roll <= cfg.p_value) {
                        // Transmit in current slot
                        attempted_in_p = true;
                        channel.handle_start_tx(station_id, frame_seq, static_cast<uint32_t>(cfg.tx_time_ms), ep);
                        precise_sleep_ms(cfg.tx_time_ms);

                        bool success = channel.handle_end_tx(station_id, frame_seq, cfg.payload_size);
                        if (success) {
                            delivered = true;
                            result.frames_successful++;
                            double delay = (get_current_time_us() - start_us) / 1000.0;
                            result.delays.push_back(delay);
                            result.total_delay_ms += delay;
                        } else {
                            result.total_collisions++;
                            retry++;
                            double backoff = compute_beb(retry);
                            precise_sleep_ms(backoff);
                        }
                    } else {
                        // Defer for 1 slot
                        precise_sleep_ms(cfg.slot_time_ms);
                        if (channel.handle_sense(station_id) != ChannelState::IDLE) {
                            // Another station seized the channel; wait until it clears
                            break;
                        }
                    }
                }
            } else if (cfg.protocol == ProtocolType::CSMA_CD) {
                // CSMA/CD: 1-persistent sensing + Listen-While-Talk + Jamming
                while (channel.handle_sense(station_id) != ChannelState::IDLE) {
                    precise_sleep_ms(std::max(0.05, cfg.slot_time_ms / 10.0));
                }

                channel.handle_start_tx(station_id, frame_seq, static_cast<uint32_t>(cfg.tx_time_ms), ep);

                double elapsed_tx_ms = 0.0;
                double check_step_ms = std::max(0.1, cfg.slot_time_ms / 5.0);
                bool collision_detected = false;

                while (elapsed_tx_ms < cfg.tx_time_ms) {
                    precise_sleep_ms(check_step_ms);
                    elapsed_tx_ms += check_step_ms;

                    if (channel.handle_check_cd(station_id)) {
                        collision_detected = true;
                        break;
                    }
                }

                if (collision_detected) {
                    // Instantly abort transmission & broadcast Jam
                    channel.handle_jam(station_id);
                    precise_sleep_ms(cfg.jam_time_ms);
                    channel.handle_end_tx(station_id, frame_seq, 0);

                    result.total_collisions++;
                    result.total_jams++;
                    retry++;

                    double backoff = compute_beb(retry);
                    precise_sleep_ms(backoff);
                } else {
                    bool success = channel.handle_end_tx(station_id, frame_seq, cfg.payload_size);
                    if (success) {
                        delivered = true;
                        result.frames_successful++;
                        double delay = (get_current_time_us() - start_us) / 1000.0;
                        result.delays.push_back(delay);
                        result.total_delay_ms += delay;
                    } else {
                        result.total_collisions++;
                        retry++;
                        double backoff = compute_beb(retry);
                        precise_sleep_ms(backoff);
                    }
                }
            }
        }

        if (!delivered) {
            result.frames_dropped++;
        }

        // Inter-Frame Gap (IFS) to allow medium arbitration
        precise_sleep_ms(cfg.slot_time_ms);
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -p, --protocol <proto>    CSMA Protocol: np | 1p | pp | cd (default: np)\n"
              << "  -N, --stations <num>      Number of contending stations (default: 5)\n"
              << "  --prob <val>              p value for p-persistent (0.01 - 1.0, default: 0.1)\n"
              << "  -n, --frames <num>        Frames per station (default: 20)\n"
              << "  -s, --size <bytes>        Frame payload size in bytes (default: 64)\n"
              << "  --slot <ms>               Slot time tau in ms (default: 1.0)\n"
              << "  --txtime <ms>             Frame transmission time in ms (default: 5.0)\n"
              << "  --jamtime <ms>            Jam signal time in ms (default: 1.0)\n"
              << "  --csv                     Print output in CSV format\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    SimConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--protocol") && i + 1 < argc) {
            std::string proto = argv[++i];
            if (proto == "np" || proto == "non_persistent" || proto == "non-persistent") cfg.protocol = ProtocolType::NON_PERSISTENT;
            else if (proto == "1p" || proto == "1_persistent" || proto == "1-persistent") cfg.protocol = ProtocolType::ONE_PERSISTENT;
            else if (proto == "pp" || proto == "p_persistent" || proto == "p-persistent") cfg.protocol = ProtocolType::P_PERSISTENT;
            else if (proto == "cd" || proto == "csma_cd" || proto == "csma-cd") cfg.protocol = ProtocolType::CSMA_CD;
        } else if ((arg == "-N" || arg == "--stations") && i + 1 < argc) {
            cfg.num_stations = std::stoi(argv[++i]);
        } else if (arg == "--prob" && i + 1 < argc) {
            cfg.p_value = std::stod(argv[++i]);
        } else if ((arg == "-n" || arg == "--frames") && i + 1 < argc) {
            cfg.frames_per_station = std::stoi(argv[++i]);
        } else if ((arg == "-s" || arg == "--size") && i + 1 < argc) {
            cfg.payload_size = std::stoi(argv[++i]);
        } else if (arg == "--slot" && i + 1 < argc) {
            cfg.slot_time_ms = std::stod(argv[++i]);
        } else if (arg == "--txtime" && i + 1 < argc) {
            cfg.tx_time_ms = std::stod(argv[++i]);
        } else if (arg == "--jamtime" && i + 1 < argc) {
            cfg.jam_time_ms = std::stod(argv[++i]);
        } else if (arg == "--csv") {
            cfg.csv_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    Channel channel;
    channel.set_verbose(false);
    channel.set_prop_delay_ms(cfg.slot_time_ms);

    std::vector<SimStationResult> results(cfg.num_stations);
    std::vector<std::thread> threads;
    threads.reserve(cfg.num_stations);

    std::atomic<bool> start_flag(false);

    for (int i = 0; i < cfg.num_stations; ++i) {
        threads.emplace_back(run_station_sim, i + 1, std::cref(cfg), std::ref(channel), 
                             std::ref(results[i]), std::ref(start_flag));
    }

    uint64_t sim_start_us = get_current_time_us();
    start_flag.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    uint64_t sim_end_us = get_current_time_us();
    double total_sim_ms = (sim_end_us - sim_start_us) / 1000.0;

    // Aggregate statistics
    int total_attempted = 0;
    int total_successful = 0;
    int total_dropped = 0;
    int total_collisions = 0;
    int total_jams = 0;
    double sum_delay = 0.0;
    int delay_count = 0;

    for (const auto& r : results) {
        total_attempted += r.frames_attempted;
        total_successful += r.frames_successful;
        total_dropped += r.frames_dropped;
        total_collisions += r.total_collisions;
        total_jams += r.total_jams;
        sum_delay += r.total_delay_ms;
        delay_count += static_cast<int>(r.delays.size());
    }

    double avg_delay_ms = (delay_count > 0) ? (sum_delay / delay_count) : 0.0;
    uint64_t total_payload_bytes = static_cast<uint64_t>(total_successful) * cfg.payload_size;
    double throughput_kbps = (total_sim_ms > 0) ? (total_payload_bytes * 8.0 / total_sim_ms) : 0.0;
    ChannelStats ch_stats = channel.get_stats();

    // Calculate Transmission Efficiency
    int total_tx_events = total_successful + total_collisions;
    double tx_efficiency = (total_tx_events > 0) ? (static_cast<double>(total_successful) / total_tx_events) * 100.0 : 0.0;

    if (cfg.csv_mode) {
        // Protocol,Stations,P,FramesPerStation,TotalAttempted,Successful,Dropped,Collisions,Jams,AvgDelayMs,ThroughputKbps,ChannelUtilizationPct,TxEfficiencyPct,SimDurationMs
        std::cout << protocol_type_to_str(cfg.protocol) << ","
                  << cfg.num_stations << ","
                  << cfg.p_value << ","
                  << cfg.frames_per_station << ","
                  << total_attempted << ","
                  << total_successful << ","
                  << total_dropped << ","
                  << total_collisions << ","
                  << total_jams << ","
                  << std::fixed << std::setprecision(2) << avg_delay_ms << ","
                  << throughput_kbps << ","
                  << ch_stats.channel_utilization << ","
                  << tx_efficiency << ","
                  << total_sim_ms << "\n";
    } else {
        std::cout << "========================================================\n"
                  << "        CSMA SIMULATION BENCHMARK SUMMARY               \n"
                  << "========================================================\n"
                  << "Protocol                : " << protocol_type_to_str(cfg.protocol) << "\n"
                  << "Contending Stations     : " << cfg.num_stations << "\n";
        if (cfg.protocol == ProtocolType::P_PERSISTENT) {
            std::cout << "Probability (p)         : " << cfg.p_value << "\n";
        }
        std::cout << "Frames per Station      : " << cfg.frames_per_station << " (Total: " << total_attempted << ")\n"
                  << "Slot Time (tau)         : " << cfg.slot_time_ms << " ms\n"
                  << "Tx Time (T_tx)          : " << cfg.tx_time_ms << " ms\n"
                  << "--------------------------------------------------------\n"
                  << "Successful Frames       : " << total_successful << " / " << total_attempted 
                  << " (" << (total_attempted > 0 ? (total_successful * 100.0 / total_attempted) : 0.0) << "%)\n"
                  << "Dropped Frames          : " << total_dropped << "\n"
                  << "Total Collisions        : " << total_collisions << "\n"
                  << "Jam Signals (CD)        : " << total_jams << "\n"
                  << "Average Delay per Pkt   : " << std::fixed << std::setprecision(2) << avg_delay_ms << " ms\n"
                  << "Effective Throughput    : " << throughput_kbps << " kbps\n"
                  << "Protocol Tx Efficiency  : " << std::fixed << std::setprecision(2) << tx_efficiency << " %\n"
                  << "Channel Air Utilization : " << std::fixed << std::setprecision(2) << ch_stats.channel_utilization << " %\n"
                  << "Total Simulation Time   : " << total_sim_ms << " ms\n"
                  << "========================================================\n";
    }

    return 0;
}
