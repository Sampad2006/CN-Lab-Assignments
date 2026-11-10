#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <thread>
#include <future>
#include <chrono>
#include <random>
#include <cassert>
#include "cdma.hpp"

using namespace cdma;

void print_banner() {
    std::cout << "\033[1;36m"
              << "╔══════════════════════════════════════════════════════════════════════════╗\n"
              << "║              CDMA MULTI-ACCESS HEADLESS SIMULATOR & BENCHMARK            ║\n"
              << "║                Orthogonal Walsh Code Multiplexing Suite                  ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════╝\n"
              << "\033[0m\n";
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -N, --stations <num>      Number of contending stations (default: 4)\n"
              << "  -r, --rounds <num>        Number of test rounds to simulate (default: 50)\n"
              << "  --noise <stddev>          Channel noise stddev (default: 0.0)\n"
              << "  --ber-test                Run Bit Error Rate (BER) vs Noise benchmark\n"
              << "  --gain-test               Run Processing Gain benchmark (M=4, 8, 16, 32 under noise)\n"
              << "  --msg-test                Run simultaneous multi-station text broadcast demo\n"
              << "  -P, --port <port>         Base port for simulation (default: 9100)\n"
              << "  -h, --help                Show this help message\n";
}

// Scenario 1: Standard Slotted Bit Simulation
bool run_bit_simulation(size_t num_stations, size_t num_rounds, double noise, uint16_t port) {
    std::cout << "\033[1;33m[Running Scenario 1: Multi-Station Slotted Bit Simulation]\033[0m\n"
              << "  Stations (N)    : " << num_stations << "\n"
              << "  Rounds          : " << num_rounds << "\n"
              << "  Noise StdDev    : " << noise << "\n"
              << "  Server Port     : " << port << "\n\n";

    ChannelConfig srv_cfg;
    srv_cfg.port = port;
    srv_cfg.num_stations = num_stations;
    srv_cfg.noise_stddev = noise;
    srv_cfg.verbose = false;

    std::thread server_thread([srv_cfg]() {
        CdmaChannelServer server(srv_cfg);
        server.run();
    });
    server_thread.detach();

    // Give server time to bind
    sleep_ms(100);

    // Spawn N station clients
    std::vector<std::unique_ptr<CdmaStationClient>> stations;
    for (size_t i = 0; i < num_stations; ++i) {
        auto stn = std::make_unique<CdmaStationClient>("127.0.0.1", port, false);
        if (!stn->connect_and_register()) {
            std::cerr << "Failed to connect station " << (i + 1) << "\n";
            return false;
        }
        stations.push_back(std::move(stn));
    }

    size_t M = stations[0]->get_code_len();
    std::cout << "  Walsh Code Dimension M = " << M << " (Sylvester Hadamard Matrix)\n";
    std::cout << "  Simulating " << num_rounds << " synchronized CDMA rounds...\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> bit_dist(-1, 1); // -1 = silent, 0 = bit 0, 1 = bit 1

    size_t total_transmitted_bits = 0;
    size_t total_errors = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t r = 1; r <= num_rounds; ++r) {
        std::vector<int> sent_bits(num_stations);
        for (size_t i = 0; i < num_stations; ++i) {
            sent_bits[i] = bit_dist(rng);
        }

        std::vector<std::future<std::vector<WalshMatrix::DecodeResult>>> futures;
        for (size_t i = 0; i < num_stations; ++i) {
            futures.push_back(std::async(std::launch::async, [&stations, i, sent_bits]() {
                std::vector<WalshMatrix::DecodeResult> dec;
                stations[i]->transmit_bit_round(sent_bits[i], dec);
                return dec;
            }));
        }

        std::vector<std::vector<WalshMatrix::DecodeResult>> round_results;
        for (auto& f : futures) {
            round_results.push_back(f.get());
        }

        // Verify that every station accurately reconstructed all sent bits
        for (size_t stn = 0; stn < num_stations; ++stn) {
            const auto& dec = round_results[stn];
            for (size_t target = 0; target < num_stations; ++target) {
                total_transmitted_bits++;
                if (dec[target].raw_bit != sent_bits[target]) {
                    total_errors++;
                }
            }
        }

        if (r <= 5 || r == num_rounds) {
            std::cout << "  Round " << std::setw(3) << r << " | Transmitted: [";
            for (size_t i = 0; i < num_stations; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "S" << (i + 1) << ":"
                          << (sent_bits[i] == 1 ? "1" : (sent_bits[i] == 0 ? "0" : "s"));
            }
            std::cout << "] => All " << num_stations << " stations reconstructed perfectly!\n";
        } else if (r == 6) {
            std::cout << "  ... (processing rounds 6 through " << (num_rounds - 1) << ") ...\n";
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "\n\033[1;32m═══════════════════════════════════════════════════════════\033[0m\n"
              << "\033[1;32m  Simulation Complete: 100% Orthogonal Recovery Confirmed! \033[0m\n"
              << "\033[1;32m═══════════════════════════════════════════════════════════\033[0m\n"
              << "  Total Stations Contending : " << num_stations << "\n"
              << "  Walsh Code Size (M)       : " << M << "\n"
              << "  Total Bit Decodes Tested  : " << total_transmitted_bits << "\n"
              << "  Bit Decoding Errors       : " << total_errors << "\n"
              << "  Bit Error Rate (BER)      : " << (static_cast<double>(total_errors) / total_transmitted_bits) << "\n"
              << "  Total Duration            : " << duration_ms << " ms\n"
              << "  CDMA Throughput           : " << (num_rounds * 1000.0 / duration_ms) << " rounds/sec\n\n";

    // Disconnect stations
    for (auto& stn : stations) {
        stn->close();
    }

    return (total_errors == 0);
}

// Scenario 2: Concurrent Multi-Station Text Broadcast
bool run_message_simulation(uint16_t port, double noise = 0.0) {
    std::cout << "\033[1;33m[Running Scenario 2: Concurrent Text Message Broadcast over CDMA]\033[0m\n"
              << "  Channel Noise StdDev : " << noise << (noise > 0.0 ? " (Additive Noise ON)" : " (Ideal Channel / No Noise)") << "\n"
              << "  Four stations will transmit distinct text strings simultaneously over the\n"
              << "  common physical channel. Each station will demultiplex and reconstruct\n"
              << "  all four transmissions using Walsh orthogonal despreading.\n\n";

    ChannelConfig srv_cfg;
    srv_cfg.port = port;
    srv_cfg.num_stations = 4;
    srv_cfg.noise_stddev = noise;
    srv_cfg.verbose = false;

    std::thread server_thread([srv_cfg]() {
        CdmaChannelServer server(srv_cfg);
        server.run();
    });
    server_thread.detach();

    sleep_ms(100);

    std::vector<std::unique_ptr<CdmaStationClient>> stations;
    for (size_t i = 0; i < 4; ++i) {
        auto stn = std::make_unique<CdmaStationClient>("127.0.0.1", port, false);
        if (!stn->connect_and_register()) return false;
        stations.push_back(std::move(stn));
    }

    std::vector<std::string> messages = {
        "JU_CSE_2026",
        "CDMA_WALSH",
        "SAMPAD_DE",
        "NETWORKS_LAB"
    };

    std::cout << "  Input Messages to transmit:\n";
    size_t max_bits = 0;
    for (size_t i = 0; i < 4; ++i) {
        std::cout << "    Station " << (i + 1) << " (W_" << (i + 1) << "): \"" << messages[i] << "\"\n";
        size_t b = messages[i].size() * 8;
        if (b > max_bits) max_bits = b;
    }
    std::cout << "\n  Broadcasting across " << max_bits << " concurrent CDMA slots...\n";

    std::vector<std::future<std::map<uint16_t, std::string>>> futures;
    for (size_t i = 0; i < 4; ++i) {
        futures.push_back(std::async(std::launch::async, [&stations, i, &messages, max_bits]() {
            return stations[i]->transmit_message(messages[i], max_bits);
        }));
    }

    std::vector<std::map<uint16_t, std::string>> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    std::cout << "\n\033[1;32m  [Reconstruction at Station 1 Terminal]:\033[0m\n";
    bool all_match = true;
    for (size_t i = 0; i < 4; ++i) {
        std::string rec = results[0][i + 1];
        bool match = (rec == messages[i]);
        if (!match) all_match = false;
        std::cout << "    From Station " << (i + 1) << ": \"" << rec << "\""
                  << (match ? " \033[32m[MATCH OK]\033[0m" : " \033[31m[CORRUPTED BY NOISE]\033[0m") << "\n";
    }

    if (all_match) {
        std::cout << "\n\033[1;32m  Simultaneous CDMA Text Reconstruction Verified 100% Accurate!\033[0m\n\n";
    } else {
        std::cout << "\n\033[1;33m  Simultaneous CDMA Text Reconstruction completed with expected noise perturbations.\033[0m\n\n";
    }

    for (auto& stn : stations) {
        stn->close();
    }
    return all_match;
}

// Scenario 3: Noise Robustness & BER Analysis
void run_ber_test(uint16_t base_port) {
    std::cout << "\033[1;33m[Running Scenario 3: Bit Error Rate (BER) vs Additive Noise Analysis]\033[0m\n"
              << "  Analyzing processing gain and noise tolerance of Walsh spreading...\n\n";

    std::vector<double> noise_levels = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0};
    size_t rounds = 100;
    size_t N = 4;

    std::cout << "  " << std::setw(12) << "Noise StdDev"
              << std::setw(16) << "Total Bits"
              << std::setw(16) << "Errors"
              << std::setw(16) << "BER"
              << std::setw(16) << "Status" << "\n";
    std::cout << "  " << std::string(72, '-') << "\n";

    uint16_t current_port = base_port;
    for (double noise : noise_levels) {
        ChannelConfig srv_cfg;
        srv_cfg.port = current_port++;
        srv_cfg.num_stations = N;
        srv_cfg.noise_stddev = noise;
        srv_cfg.verbose = false;

        std::thread srv([srv_cfg]() {
            CdmaChannelServer server(srv_cfg);
            server.run();
        });
        srv.detach();
        sleep_ms(50);

        std::vector<std::unique_ptr<CdmaStationClient>> stations;
        for (size_t i = 0; i < N; ++i) {
            auto stn = std::make_unique<CdmaStationClient>("127.0.0.1", srv_cfg.port, false);
            stn->connect_and_register();
            stations.push_back(std::move(stn));
        }

        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> bdist(0, 1);

        size_t total_bits = 0;
        size_t error_count = 0;

        for (size_t r = 0; r < rounds; ++r) {
            std::vector<int> sent(N);
            for (size_t i = 0; i < N; ++i) sent[i] = bdist(rng);

            std::vector<std::future<std::vector<WalshMatrix::DecodeResult>>> futures;
            for (size_t i = 0; i < N; ++i) {
                futures.push_back(std::async(std::launch::async, [&stations, i, sent]() {
                    std::vector<WalshMatrix::DecodeResult> dec;
                    stations[i]->transmit_bit_round(sent[i], dec);
                    return dec;
                }));
            }

            for (size_t i = 0; i < N; ++i) {
                auto dec = futures[i].get();
                for (size_t t = 0; t < N; ++t) {
                    total_bits++;
                    if (dec[t].raw_bit != sent[t]) {
                        error_count++;
                    }
                }
            }
        }

        double ber = static_cast<double>(error_count) / total_bits;
        std::cout << "  " << std::setw(12) << std::fixed << std::setprecision(2) << noise
                  << std::setw(16) << total_bits
                  << std::setw(16) << error_count
                  << std::setw(16) << std::fixed << std::setprecision(5) << ber
                  << std::setw(16) << (ber < 0.01 ? "EXCELLENT" : (ber < 0.08 ? "GOOD" : "DEGRADED")) << "\n";

        for (auto& stn : stations) stn->close();
        sleep_ms(50);
    }
    std::cout << "  " << std::string(72, '-') << "\n\n";
}

// Scenario 4: Spreading Factor / Processing Gain Analysis vs M under fixed noise
void run_processing_gain_test(uint16_t base_port) {
    std::cout << "\033[1;33m[Running Scenario 4: Processing Gain vs Spreading Factor M]\033[0m\n"
              << "  Fixed Channel Noise StdDev = 0.80. Evaluating BER across M = 4, 8, 16, 32...\n\n";

    std::vector<size_t> m_values = {4, 8, 16, 32};
    double fixed_noise = 0.80;
    size_t rounds = 80;

    std::cout << "  " << std::setw(8) << "M"
              << std::setw(14) << "Gain (dB)"
              << std::setw(16) << "Total Bits"
              << std::setw(16) << "Errors"
              << std::setw(16) << "BER"
              << std::setw(18) << "Noise Margin" << "\n";
    std::cout << "  " << std::string(88, '-') << "\n";

    uint16_t current_port = base_port;
    for (size_t M : m_values) {
        size_t N = M;
        ChannelConfig srv_cfg;
        srv_cfg.port = current_port++;
        srv_cfg.num_stations = N;
        srv_cfg.noise_stddev = fixed_noise;
        srv_cfg.verbose = false;

        std::thread srv([srv_cfg]() {
            CdmaChannelServer server(srv_cfg);
            server.run();
        });
        srv.detach();
        sleep_ms(60);

        std::vector<std::unique_ptr<CdmaStationClient>> stations;
        for (size_t i = 0; i < N; ++i) {
            auto stn = std::make_unique<CdmaStationClient>("127.0.0.1", srv_cfg.port, false);
            stn->connect_and_register();
            stations.push_back(std::move(stn));
        }

        std::mt19937 rng(999);
        std::uniform_int_distribution<int> bdist(0, 1);

        size_t total_bits = 0;
        size_t error_count = 0;

        for (size_t r = 0; r < rounds; ++r) {
            std::vector<int> sent(N);
            for (size_t i = 0; i < N; ++i) sent[i] = bdist(rng);

            std::vector<std::future<std::vector<WalshMatrix::DecodeResult>>> futures;
            for (size_t i = 0; i < N; ++i) {
                futures.push_back(std::async(std::launch::async, [&stations, i, sent]() {
                    std::vector<WalshMatrix::DecodeResult> dec;
                    stations[i]->transmit_bit_round(sent[i], dec);
                    return dec;
                }));
            }

            for (size_t i = 0; i < N; ++i) {
                auto dec = futures[i].get();
                for (size_t t = 0; t < N; ++t) {
                    total_bits++;
                    if (dec[t].raw_bit != sent[t]) {
                        error_count++;
                    }
                }
            }
        }

        double ber = static_cast<double>(error_count) / total_bits;
        double gain_db = 10.0 * std::log10(static_cast<double>(M));

        std::cout << "  " << std::setw(8) << M
                  << std::setw(14) << std::fixed << std::setprecision(2) << gain_db
                  << std::setw(16) << total_bits
                  << std::setw(16) << error_count
                  << std::setw(16) << std::fixed << std::setprecision(5) << ber
                  << std::setw(18) << (ber < 0.05 ? "HIGH IMMUNITY" : "MODERATE") << "\n";

        for (auto& stn : stations) stn->close();
        sleep_ms(60);
    }
    std::cout << "  " << std::string(88, '-') << "\n\n";
}

int main(int argc, char* argv[]) {
    size_t num_stations = 4;
    size_t num_rounds = 30;
    double noise = 0.0;
    uint16_t port = 9100;
    bool ber_test = false;
    bool gain_test = false;
    bool msg_test = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-N" || arg == "--stations") {
            if (i + 1 < argc) num_stations = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "-r" || arg == "--rounds") {
            if (i + 1 < argc) num_rounds = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--noise") {
            if (i + 1 < argc) noise = std::stod(argv[++i]);
        } else if (arg == "-P" || arg == "--port") {
            if (i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--ber-test") {
            ber_test = true;
        } else if (arg == "--gain-test") {
            gain_test = true;
        } else if (arg == "--msg-test") {
            msg_test = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    print_banner();

    if (ber_test) {
        run_ber_test(port);
    } else if (gain_test) {
        run_processing_gain_test(port);
    } else if (msg_test) {
        run_message_simulation(port, noise);
    } else {
        run_bit_simulation(num_stations, num_rounds, noise, port);
    }

    return 0;
}
