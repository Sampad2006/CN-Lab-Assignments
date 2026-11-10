#ifndef A4_CDMA_HPP
#define A4_CDMA_HPP

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include "walsh.hpp"
#include "frame.hpp"
#include "socket.hpp"

namespace cdma {

// Channel configuration options
struct ChannelConfig {
    uint16_t port = 9090;
    std::string bind_ip = "0.0.0.0";
    size_t num_stations = 4;
    double noise_stddev = 0.0; // 0.0 for noiseless ideal channel
    bool verbose = true;
};

// Information tracked by channel for each connected station
struct StationSession {
    uint16_t id;
    std::string ip;
    uint16_t port;
    std::unique_ptr<TcpSocket> socket;
    std::vector<int8_t> walsh_code;
    bool active;

    StationSession(uint16_t sid, std::unique_ptr<TcpSocket> sock, const std::string& rip, uint16_t rport)
        : id(sid), ip(rip), port(rport), socket(std::move(sock)), active(true) {}
};

// Common physical channel server orchestrator
class CdmaChannelServer {
public:
    explicit CdmaChannelServer(const ChannelConfig& config);
    ~CdmaChannelServer();

    // Starts listening, waits for all N stations to register, and enters CDMA round loop
    bool run();
    void stop();

    // Returns generated Walsh matrix
    const std::vector<std::vector<int8_t>>& get_walsh_matrix() const { return walsh_matrix_; }
    size_t get_code_len() const { return code_len_; }

private:
    bool accept_and_register_stations();
    bool execute_slotted_round(uint32_t round_seq);

    ChannelConfig config_;
    size_t code_len_; // M = next_power_of_2(N)
    std::vector<std::vector<int8_t>> walsh_matrix_;

    TcpServer server_;
    std::vector<std::unique_ptr<StationSession>> stations_;
    std::atomic<bool> running_;
    std::mutex session_mutex_;
};

// Station client controller
class CdmaStationClient {
public:
    CdmaStationClient(const std::string& host, uint16_t port, bool verbose = true);
    ~CdmaStationClient();

    // Connects to channel server and performs registration
    bool connect_and_register();

    // Single interactive round (prompts user for 1, 0, or s)
    bool run_interactive_round();

    // Interactive loop until server terminates or user quits
    void run_interactive_session();

    // Automated single-bit transmission
    bool transmit_bit_round(int raw_bit, std::vector<WalshMatrix::DecodeResult>& out_all_decoded);

    // Automated full text message transmission across multiple synchronous CDMA slots
    std::map<uint16_t, std::string> transmit_message(
        const std::string& message,
        size_t total_rounds = 0,
        std::atomic<bool>* cancel_flag = nullptr
    );

    void close();

    uint16_t get_station_id() const { return station_id_; }
    uint16_t get_total_stations() const { return total_stations_; }
    size_t get_code_len() const { return code_len_; }
    const std::vector<int8_t>& get_walsh_code() const { return my_walsh_code_; }
    const std::vector<std::vector<int8_t>>& get_full_matrix() const { return walsh_matrix_; }

private:
    std::string host_;
    uint16_t port_;
    bool verbose_;

    TcpSocket socket_;
    uint16_t station_id_;
    uint16_t total_stations_;
    size_t code_len_;
    std::vector<int8_t> my_walsh_code_;
    std::vector<std::vector<int8_t>> walsh_matrix_;
    bool registered_;
};

} // namespace cdma

#endif // A4_CDMA_HPP
