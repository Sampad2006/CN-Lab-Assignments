#include "cdma.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <chrono>

namespace cdma {

// ANSI Color Codes for terminal telemetry
namespace Color {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* RED     = "\033[31m";
    const char* GREEN   = "\033[32m";
    const char* YELLOW  = "\033[33m";
    const char* BLUE    = "\033[34m";
    const char* MAGENTA = "\033[35m";
    const char* CYAN    = "\033[36m";
}

// ─────────────────────────────────────────────────────────────────────────────
// CdmaChannelServer Implementation
// ─────────────────────────────────────────────────────────────────────────────

CdmaChannelServer::CdmaChannelServer(const ChannelConfig& config)
    : config_(config), running_(false) {
    code_len_ = WalshMatrix::next_power_of_2(config_.num_stations);
    walsh_matrix_ = WalshMatrix::generate(code_len_);

    if (!WalshMatrix::verify_orthogonality(walsh_matrix_)) {
        throw std::runtime_error("Generated Walsh Matrix failed strict orthogonality invariant check!");
    }
}

CdmaChannelServer::~CdmaChannelServer() {
    stop();
}

void CdmaChannelServer::stop() {
    running_ = false;
    server_.close();
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& session : stations_) {
        if (session && session->socket) {
            session->socket->close();
        }
    }
}

bool CdmaChannelServer::run() {
    if (!server_.listen_on(config_.port, config_.bind_ip)) {
        std::cerr << Color::RED << "[Server Error] Failed to listen on "
                  << config_.bind_ip << ":" << config_.port << Color::RESET << "\n";
        return false;
    }

    running_ = true;

    if (config_.verbose) {
        std::cout << Color::CYAN << Color::BOLD
                  << "╔══════════════════════════════════════════════════════════════════════════╗\n"
                  << "║                  CDMA COMMON PHYSICAL CHANNEL SERVER                     ║\n"
                  << "╠══════════════════════════════════════════════════════════════════════════╣\n"
                  << Color::RESET
                  << "  Binding Address       : " << config_.bind_ip << ":" << config_.port << "\n"
                  << "  Expected Stations (N) : " << config_.num_stations << "\n"
                  << "  Walsh Code Size   (M) : " << code_len_ << " (Orthogonal Dimension)\n"
                  << "  Channel Noise StdDev  : " << config_.noise_stddev << (config_.noise_stddev > 0.0 ? " (Additive Noise ON)" : " (Ideal Channel)") << "\n"
                  << "  Orthogonality Verified: " << Color::GREEN << "STRICTLY ORTHOGONAL (W_i . W_j = " << code_len_ << " * delta_ij)" << Color::RESET << "\n"
                  << "────────────────────────────────────────────────────────────────────────────\n"
                  << "  Walsh Code Allocation Table:\n";

        for (size_t i = 0; i < code_len_; ++i) {
            if (i < config_.num_stations) {
                std::cout << "    Station " << (i + 1) << " (W_" << (i + 1) << "): "
                          << WalshMatrix::format_chips(walsh_matrix_[i]) << "\n";
            } else {
                std::cout << "    Spare     (W_" << (i + 1) << "): "
                          << WalshMatrix::format_chips(walsh_matrix_[i]) << " [Unassigned]\n";
            }
        }
        std::cout << Color::CYAN << Color::BOLD
                  << "╚══════════════════════════════════════════════════════════════════════════╝\n"
                  << Color::RESET
                  << "\nWaiting for " << config_.num_stations << " stations to connect and register...\n";
    }

    if (!accept_and_register_stations()) {
        std::cerr << Color::RED << "[Server Error] Station registration phase failed.\n" << Color::RESET;
        return false;
    }

    if (config_.verbose) {
        std::cout << "\n" << Color::GREEN << Color::BOLD
                  << "============================================================================\n"
                  << " All " << config_.num_stations << " Stations Connected & Synchronized! Entering CDMA Slotted Rounds\n"
                  << "============================================================================\n"
                  << Color::RESET;
    }

    uint32_t round_seq = 1;
    while (running_) {
        if (!execute_slotted_round(round_seq++)) {
            break;
        }
    }

    return true;
}

bool CdmaChannelServer::accept_and_register_stations() {
    stations_.clear();
    stations_.reserve(config_.num_stations);

    for (uint16_t sid = 1; sid <= config_.num_stations; ++sid) {
        std::string client_ip;
        uint16_t client_port = 0;
        auto client_sock = server_.accept_client(client_ip, client_port);
        if (!client_sock) {
            if (!running_) return false;
            std::cerr << Color::RED << "[Server Error] Failed to accept station " << sid << Color::RESET << "\n";
            return false;
        }

        Packet req;
        if (!client_sock->recv_packet(req) || req.type != MsgType::REGISTER_REQ) {
            std::cerr << Color::RED << "[Server Error] Invalid handshake from " << client_ip << Color::RESET << "\n";
            return false;
        }

        const auto& code = walsh_matrix_[sid - 1];
        auto session = std::make_unique<StationSession>(sid, std::move(client_sock), client_ip, client_port);
        session->walsh_code = code;

        // 1. Send Registration Response
        RegisterRespData resp_data;
        resp_data.assigned_id = sid;
        resp_data.code_len = static_cast<uint16_t>(code_len_);
        resp_data.total_stations = static_cast<uint16_t>(config_.num_stations);
        resp_data.walsh_code = code;

        Packet resp_pkt(MsgType::REGISTER_RESP, sid, 0, FrameCodec::encode_register_resp(resp_data));
        if (!session->socket->send_packet(resp_pkt)) {
            std::cerr << Color::RED << "[Server Error] Failed to send REGISTER_RESP to Station " << sid << Color::RESET << "\n";
            return false;
        }

        // 2. Send Config Sync (Full Walsh Matrix)
        ConfigSyncData sync_data;
        sync_data.code_len = static_cast<uint16_t>(code_len_);
        sync_data.total_stations = static_cast<uint16_t>(config_.num_stations);
        sync_data.walsh_matrix = walsh_matrix_;

        Packet sync_pkt(MsgType::CONFIG_SYNC, sid, 0, FrameCodec::encode_config_sync(sync_data));
        if (!session->socket->send_packet(sync_pkt)) {
            std::cerr << Color::RED << "[Server Error] Failed to send CONFIG_SYNC to Station " << sid << Color::RESET << "\n";
            return false;
        }

        if (config_.verbose) {
            std::cout << Color::GREEN << "  [+] Station " << sid << Color::RESET
                      << " connected from " << client_ip << ":" << client_port
                      << " | Assigned W_" << sid << ": " << WalshMatrix::format_chips(code) << "\n";
        }

        stations_.push_back(std::move(session));
    }

    return true;
}

bool CdmaChannelServer::execute_slotted_round(uint32_t round_seq) {
    if (config_.verbose) {
        std::cout << "\n" << Color::YELLOW << Color::BOLD
                  << "─── [CDMA Slotted Round #" << round_seq << "] ────────────────────────────────────────"
                  << Color::RESET << "\n"
                  << "Signaling " << stations_.size() << " stations to modulate and transmit chips...\n";
    }

    // Step 1: Signal all stations that the round has started
    Packet start_pkt(MsgType::SLOT_START, 0, round_seq);
    for (auto& session : stations_) {
        if (!session->socket->send_packet(start_pkt)) {
            if (config_.verbose) {
                std::cout << "[Server] Station " << session->id << " disconnected during SLOT_START.\n";
            }
            return false;
        }
    }

    // Step 2: Concurrently collect chip sequence from all stations
    struct StationChipResult {
        uint16_t id = 0;
        bool success = false;
        std::vector<int8_t> chips;
    };

    std::vector<StationChipResult> results(stations_.size());
    std::vector<std::thread> workers;
    workers.reserve(stations_.size());

    for (size_t i = 0; i < stations_.size(); ++i) {
        workers.emplace_back([this, i, &results]() {
            auto& session = stations_[i];
            Packet pkt;
            if (!session->socket->recv_packet(pkt)) {
                results[i].success = false;
                return;
            }

            if (pkt.type == MsgType::DISCONNECT) {
                results[i].success = false;
                return;
            }

            if (pkt.type != MsgType::CHIP_TRANSMIT) {
                results[i].success = false;
                return;
            }

            ChipTransmitData tx_data;
            if (!FrameCodec::decode_chip_transmit(pkt.payload, tx_data)) {
                results[i].success = false;
                return;
            }

            results[i].id = tx_data.station_id;
            results[i].chips = tx_data.chips;
            results[i].success = true;
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    std::vector<std::vector<int8_t>> collected_vectors;
    collected_vectors.reserve(stations_.size());

    for (size_t i = 0; i < stations_.size(); ++i) {
        if (!results[i].success) {
            if (config_.verbose) {
                std::cout << "[Server] Station " << stations_[i]->id << " finished / disconnected.\n";
            }
            return false;
        }
        collected_vectors.push_back(results[i].chips);
        if (config_.verbose) {
            std::cout << "  <- Station " << results[i].id << " Transmitted Chips C_" << results[i].id << ": "
                      << WalshMatrix::format_chips(results[i].chips) << "\n";
        }
    }

    // Step 3: Medium Linear Superposition: S = SUM(C_i)
    std::vector<int32_t> composite = WalshMatrix::superimpose(collected_vectors, code_len_);

    // Apply channel noise if configured
    if (config_.noise_stddev > 0.0) {
        composite = WalshMatrix::add_channel_noise(composite, config_.noise_stddev);
    }

    if (config_.verbose) {
        std::cout << "  " << std::string(68, '-') << "\n"
                  << Color::CYAN << Color::BOLD
                  << "  [Channel Superposition S = sum C_i]: "
                  << WalshMatrix::format_signal(composite) << Color::RESET << "\n"
                  << "  " << std::string(68, '-') << "\n";
    }

    // Step 4: Broadcast composite channel signal S to all stations
    ChannelBroadcastData bcast_data;
    bcast_data.round_seq = round_seq;
    bcast_data.code_len = static_cast<uint16_t>(code_len_);
    bcast_data.composite_signal = composite;

    Packet bcast_pkt(
        MsgType::CHANNEL_BROADCAST,
        0,
        round_seq,
        FrameCodec::encode_channel_broadcast(bcast_data)
    );

    for (auto& session : stations_) {
        if (!session->socket->send_packet(bcast_pkt)) {
            std::cerr << Color::RED << "[Server] Failed to broadcast channel state to Station " << session->id << Color::RESET << "\n";
            return false;
        }
    }

    if (config_.verbose) {
        std::cout << "  => Composite signal S successfully broadcasted to all " << stations_.size() << " stations.\n";
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CdmaStationClient Implementation
// ─────────────────────────────────────────────────────────────────────────────

CdmaStationClient::CdmaStationClient(const std::string& host, uint16_t port, bool verbose)
    : host_(host), port_(port), verbose_(verbose),
      station_id_(0), total_stations_(0), code_len_(0), registered_(false) {}

CdmaStationClient::~CdmaStationClient() {
    close();
}

void CdmaStationClient::close() {
    if (socket_.is_open()) {
        Packet quit_pkt(MsgType::DISCONNECT, station_id_, 0);
        socket_.send_packet(quit_pkt);
        socket_.close();
    }
}

bool CdmaStationClient::connect_and_register() {
    if (!socket_.connect_to(host_, port_)) {
        std::cerr << Color::RED << "[Error] Could not connect to channel server at " << host_ << ":" << port_ << Color::RESET << "\n";
        return false;
    }

    // Send REGISTER_REQ
    Packet req(MsgType::REGISTER_REQ, 0, 0);
    if (!socket_.send_packet(req)) {
        std::cerr << Color::RED << "[Error] Failed to send registration request.\n" << Color::RESET;
        return false;
    }

    // Recv REGISTER_RESP
    Packet resp;
    if (!socket_.recv_packet(resp) || resp.type != MsgType::REGISTER_RESP) {
        std::cerr << Color::RED << "[Error] Unexpected response to registration.\n" << Color::RESET;
        return false;
    }

    RegisterRespData reg_data;
    if (!FrameCodec::decode_register_resp(resp.payload, reg_data)) {
        std::cerr << Color::RED << "[Error] Failed to decode registration response.\n" << Color::RESET;
        return false;
    }

    station_id_ = reg_data.assigned_id;
    code_len_ = reg_data.code_len;
    total_stations_ = reg_data.total_stations;
    my_walsh_code_ = reg_data.walsh_code;

    // Recv CONFIG_SYNC
    Packet sync_pkt;
    if (!socket_.recv_packet(sync_pkt) || sync_pkt.type != MsgType::CONFIG_SYNC) {
        std::cerr << Color::RED << "[Error] Failed to receive CONFIG_SYNC from server.\n" << Color::RESET;
        return false;
    }

    ConfigSyncData sync_data;
    if (!FrameCodec::decode_config_sync(sync_pkt.payload, sync_data)) {
        std::cerr << Color::RED << "[Error] Failed to decode CONFIG_SYNC payload.\n" << Color::RESET;
        return false;
    }
    walsh_matrix_ = sync_data.walsh_matrix;
    registered_ = true;

    if (verbose_) {
        std::cout << Color::CYAN << Color::BOLD
                  << "╔══════════════════════════════════════════════════════════════════════════╗\n"
                  << "║                       CDMA STATION REGISTERED                            ║\n"
                  << "╠══════════════════════════════════════════════════════════════════════════╣\n"
                  << Color::RESET
                  << "  Station Identity (ID)   : " << Color::GREEN << Color::BOLD << station_id_ << Color::RESET << "\n"
                  << "  Total Stations in Net   : " << total_stations_ << "\n"
                  << "  Walsh Code Length (M)   : " << code_len_ << "\n"
                  << "  Assigned Walsh Code W_" << station_id_ << ": " << WalshMatrix::format_chips(my_walsh_code_) << "\n"
                  << Color::CYAN << Color::BOLD
                  << "╚══════════════════════════════════════════════════════════════════════════╝\n"
                  << Color::RESET;
    }

    return true;
}

bool CdmaStationClient::run_interactive_round() {
    Packet start_pkt;
    if (!socket_.recv_packet(start_pkt)) {
        std::cout << Color::RED << "Channel server disconnected.\n" << Color::RESET;
        return false;
    }

    if (start_pkt.type != MsgType::SLOT_START) {
        std::cerr << Color::RED << "Unexpected packet received instead of SLOT_START\n" << Color::RESET;
        return false;
    }

    uint32_t round_seq = start_pkt.round_seq;
    std::cout << "\n" << Color::YELLOW << Color::BOLD
              << "─── [Slotted Round #" << round_seq << " | Station " << station_id_ << "] ───────────────────────────────"
              << Color::RESET << "\n";

    std::cout << "Enter bit to transmit (1 = bit 1, 0 = bit 0, s = silent, q = quit) [default: s]: ";
    std::string input;
    if (!std::getline(std::cin, input)) {
        return false;
    }

    if (input == "q" || input == "Q") {
        return false;
    }

    int raw_bit = -1; // silent by default
    BitState state = BitState::SILENT;
    if (input == "1") {
        raw_bit = 1;
        state = BitState::BIT_ONE;
    } else if (input == "0") {
        raw_bit = 0;
        state = BitState::BIT_ZERO;
    }

    // Modulate / Spread Bit
    std::vector<int8_t> chips = WalshMatrix::spread_bit(state, my_walsh_code_);

    if (state == BitState::SILENT) {
        std::cout << "  -> [Tx] Station " << station_id_ << " is SILENT. Sending all-zero chip vector: "
                  << WalshMatrix::format_chips(chips) << "\n";
    } else {
        int bipolar = (state == BitState::BIT_ONE) ? +1 : -1;
        std::cout << "  -> [Tx] Bit " << raw_bit << " (bipolar " << (bipolar > 0 ? "+1" : "-1")
                  << ") * W_" << station_id_ << " = " << WalshMatrix::format_chips(chips) << "\n";
    }

    // Transmit chips to channel server
    ChipTransmitData tx_data;
    tx_data.station_id = station_id_;
    tx_data.round_seq = round_seq;
    tx_data.chips = chips;

    Packet tx_pkt(MsgType::CHIP_TRANSMIT, station_id_, round_seq, FrameCodec::encode_chip_transmit(tx_data));
    if (!socket_.send_packet(tx_pkt)) {
        std::cerr << Color::RED << "[Error] Failed to send chips to channel server.\n" << Color::RESET;
        return false;
    }

    std::cout << "  Waiting for common channel superposition broadcast...\n";

    // Receive channel broadcast
    Packet bcast_pkt;
    if (!socket_.recv_packet(bcast_pkt) || bcast_pkt.type != MsgType::CHANNEL_BROADCAST) {
        std::cerr << Color::RED << "[Error] Failed to receive channel broadcast.\n" << Color::RESET;
        return false;
    }

    ChannelBroadcastData bcast_data;
    if (!FrameCodec::decode_channel_broadcast(bcast_pkt.payload, bcast_data)) {
        std::cerr << Color::RED << "[Error] Failed to decode broadcast payload.\n" << Color::RESET;
        return false;
    }

    const auto& S = bcast_data.composite_signal;

    std::cout << "  " << std::string(68, '-') << "\n"
              << Color::CYAN << "  [Rx] Channel Superimposed Composite Signal S: "
              << WalshMatrix::format_signal(S) << Color::RESET << "\n"
              << "  " << std::string(68, '-') << "\n";

    // Reconstruct data sent by all stations
    std::cout << Color::BOLD << "  [Mathematical Reconstruction Across All Stations]:" << Color::RESET << "\n";
    for (uint16_t sid = 1; sid <= total_stations_; ++sid) {
        const auto& code = walsh_matrix_[sid - 1];
        auto dec = WalshMatrix::decode(S, code);

        std::string tag = (sid == station_id_) ? " (You)" : "";
        std::string val_str;
        if (dec.raw_bit == 1) {
            val_str = Color::GREEN + std::string("Bit 1") + Color::RESET;
        } else if (dec.raw_bit == 0) {
            val_str = Color::RED + std::string("Bit 0") + Color::RESET;
        } else {
            val_str = Color::YELLOW + std::string("Silent (Idle)") + Color::RESET;
        }

        std::cout << "    Station " << sid << tag << ": InnerProduct (S . W_" << sid << ") = "
                  << std::setw(3) << dec.dot_product
                  << " | Normalized (Sum / " << code_len_ << ") = "
                  << std::fixed << std::setprecision(1) << dec.normalized
                  << " => " << val_str << "\n";
    }

    return true;
}

void CdmaStationClient::run_interactive_session() {
    while (run_interactive_round()) {
        // Continue round loop
    }
}

bool CdmaStationClient::transmit_bit_round(int raw_bit, std::vector<WalshMatrix::DecodeResult>& out_all_decoded) {
    Packet start_pkt;
    if (!socket_.recv_packet(start_pkt) || start_pkt.type != MsgType::SLOT_START) {
        return false;
    }

    uint32_t round_seq = start_pkt.round_seq;
    std::vector<int8_t> chips = WalshMatrix::spread_bit(raw_bit, my_walsh_code_);

    ChipTransmitData tx_data;
    tx_data.station_id = station_id_;
    tx_data.round_seq = round_seq;
    tx_data.chips = chips;

    Packet tx_pkt(MsgType::CHIP_TRANSMIT, station_id_, round_seq, FrameCodec::encode_chip_transmit(tx_data));
    if (!socket_.send_packet(tx_pkt)) {
        return false;
    }

    Packet bcast_pkt;
    if (!socket_.recv_packet(bcast_pkt) || bcast_pkt.type != MsgType::CHANNEL_BROADCAST) {
        return false;
    }

    ChannelBroadcastData bcast_data;
    if (!FrameCodec::decode_channel_broadcast(bcast_pkt.payload, bcast_data)) {
        return false;
    }

    out_all_decoded.clear();
    out_all_decoded.reserve(total_stations_);
    for (uint16_t sid = 1; sid <= total_stations_; ++sid) {
        out_all_decoded.push_back(WalshMatrix::decode(bcast_data.composite_signal, walsh_matrix_[sid - 1]));
    }

    return true;
}

std::map<uint16_t, std::string> CdmaStationClient::transmit_message(
    const std::string& message,
    size_t total_rounds,
    std::atomic<bool>* cancel_flag
) {
    std::vector<int> my_bits = WalshMatrix::string_to_bits(message);
    if (total_rounds == 0) {
        total_rounds = my_bits.size();
    }
    std::map<uint16_t, std::vector<int>> recovered_bits;
    for (uint16_t sid = 1; sid <= total_stations_; ++sid) {
        recovered_bits[sid] = {};
    }

    for (size_t r = 0; r < total_rounds; ++r) {
        if (cancel_flag && *cancel_flag) break;

        int send_bit = (r < my_bits.size()) ? my_bits[r] : -1; // -1 = silent once finished
        std::vector<WalshMatrix::DecodeResult> round_results;
        if (!transmit_bit_round(send_bit, round_results)) {
            break;
        }

        for (uint16_t sid = 1; sid <= total_stations_; ++sid) {
            recovered_bits[sid].push_back(round_results[sid - 1].raw_bit);
        }
    }

    std::map<uint16_t, std::string> recovered_messages;
    for (uint16_t sid = 1; sid <= total_stations_; ++sid) {
        recovered_messages[sid] = WalshMatrix::bits_to_string(recovered_bits[sid]);
    }

    return recovered_messages;
}

} // namespace cdma
