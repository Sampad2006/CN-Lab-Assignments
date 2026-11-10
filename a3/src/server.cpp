#include <iostream>
#include <iomanip>
#include <string>
#include <csignal>
#include <cstdlib>
#include <atomic>

#include "channel.hpp"
#include "frame.hpp"
#include "socket.hpp"

using namespace csma;

std::atomic<bool> g_running(true);

void sigint_handler(int /*signum*/) {
    g_running = false;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -P, --port <port>       Channel port/channel ID (default: 9999)\n"
              << "  -q, --quiet             Quiet mode (suppress per-event logs)\n"
              << "  -h, --help              Show this help message\n";
}

int main(int argc, char* argv[]) {
    uint16_t port = 9999;
    bool verbose = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-P" || arg == "--port") {
            if (i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-q" || arg == "--quiet") {
            verbose = false;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    UdpSocket sock;
    if (!sock.bind_auto(port, 0)) {
        std::cerr << "[Error] Failed to bind to channel socket on port/ID " << port << "\n";
        return 1;
    }

    Channel channel;
    channel.set_verbose(verbose);

    std::cout << "========================================================\n"
              << "  CN Lab Assignment 3: Shared Channel Emulator (Server) \n"
              << "========================================================\n"
              << "Bound to Channel: " << sock.get_bound_path() << " (Port/ID: " << port << ")\n"
              << "Emulating shared medium states: IDLE | BUSY | COLLISION\n"
              << "Ready to service contending station processes...\n"
              << "Press Ctrl+C to stop.\n"
              << "--------------------------------------------------------\n";

    std::vector<uint8_t> recv_buf(4096);
    Endpoint sender_ep;

    while (g_running) {
        ssize_t bytes = sock.recv_from(recv_buf.data(), recv_buf.size(), sender_ep, 100);
        if (bytes <= 0) {
            continue;
        }

        Message req;
        if (!deserialize_message(recv_buf.data(), bytes, req)) {
            continue;
        }

        Message resp;
        resp.header.station_id = req.header.station_id;
        resp.header.frame_seq = req.header.frame_seq;
        resp.header.timestamp_us = get_current_time_us();

        switch (static_cast<MsgType>(req.header.msg_type)) {
            case MsgType::MSG_SENSE_REQ: {
                ChannelState st = channel.handle_sense(req.header.station_id);
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_SENSE_RESP);
                resp.header.channel_state = static_cast<uint8_t>(st);
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_START_TX: {
                bool ok = channel.handle_start_tx(req.header.station_id, req.header.frame_seq, 
                                                 req.header.duration_ms, sender_ep);
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_START_TX);
                resp.header.channel_state = static_cast<uint8_t>(channel.get_state());
                resp.header.collided = ok ? 0 : 1;
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_CHECK_CD: {
                bool collided = channel.handle_check_cd(req.header.station_id);
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_CHECK_CD);
                resp.header.channel_state = static_cast<uint8_t>(channel.get_state());
                resp.header.collided = collided ? 1 : 0;
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_END_TX: {
                bool success = channel.handle_end_tx(req.header.station_id, req.header.frame_seq, 
                                                    static_cast<uint32_t>(req.payload.size()));
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_END_TX);
                resp.header.channel_state = static_cast<uint8_t>(channel.get_state());
                resp.header.collided = success ? 0 : 1;
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_JAM: {
                channel.handle_jam(req.header.station_id);
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_JAM);
                resp.header.channel_state = static_cast<uint8_t>(ChannelState::COLLISION);
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_STATS_REQ: {
                ChannelStats st = channel.get_stats();
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_STATS_RESP);
                resp.header.extra_info = static_cast<uint32_t>(st.collision_events);
                
                std::string stats_str = 
                    "TotalSense:" + std::to_string(st.total_sense_requests) + ";" +
                    "TotalTx:" + std::to_string(st.total_tx_attempts) + ";" +
                    "Success:" + std::to_string(st.successful_frames) + ";" +
                    "Collided:" + std::to_string(st.collided_frames) + ";" +
                    "Collisions:" + std::to_string(st.collision_events) + ";" +
                    "Jams:" + std::to_string(st.jam_signals) + ";" +
                    "BytesDelivered:" + std::to_string(st.total_bytes_delivered) + ";" +
                    "SimMs:" + std::to_string(st.total_sim_time_ms) + ";" +
                    "BusyMs:" + std::to_string(st.total_busy_time_ms) + ";" +
                    "CollisionMs:" + std::to_string(st.total_collision_time_ms) + ";" +
                    "ThroughputKbps:" + std::to_string(st.throughput_kbps) + ";" +
                    "Utilization:" + std::to_string(st.channel_utilization);
                resp.payload = stats_str;

                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            case MsgType::MSG_RESET_REQ: {
                channel.reset();
                resp.header.msg_type = static_cast<uint8_t>(MsgType::MSG_RESET_RESP);
                resp.header.channel_state = static_cast<uint8_t>(ChannelState::IDLE);
                std::vector<uint8_t> out = serialize_message(resp);
                sock.send_to(out, sender_ep);
                break;
            }

            default:
                break;
        }
    }

    ChannelStats final_stats = channel.get_stats();
    std::cout << "\n========================================================\n"
              << "                FINAL CHANNEL STATISTICS                \n"
              << "========================================================\n"
              << "Total Sense Queries Handled : " << final_stats.total_sense_requests << "\n"
              << "Total Transmission Attempts : " << final_stats.total_tx_attempts << "\n"
              << "Successful Frames Delivered : " << final_stats.successful_frames << "\n"
              << "Collided Frames             : " << final_stats.collided_frames << "\n"
              << "Total Collision Events      : " << final_stats.collision_events << "\n"
              << "Jam Signals Emitted         : " << final_stats.jam_signals << "\n"
              << "Total Bytes Delivered       : " << final_stats.total_bytes_delivered << " bytes\n"
              << "Total Simulation Time       : " << std::fixed << std::setprecision(2) 
              << final_stats.total_sim_time_ms << " ms\n"
              << "Channel Busy Time           : " << final_stats.total_busy_time_ms << " ms\n"
              << "Channel Collision Time      : " << final_stats.total_collision_time_ms << " ms\n"
              << "Effective Throughput        : " << final_stats.throughput_kbps << " kbps\n"
              << "Channel Efficiency / Util.  : " << (final_stats.channel_utilization * 100.0) << " %\n"
              << "========================================================\n";

    sock.close();
    return 0;
}
