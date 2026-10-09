#include "stop_and_wait.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;

namespace net {

StopAndWaitSender::StopAndWaitSender(UdpSocket& socket, const Endpoint& receiver_ep,
                                     const ProtocolConfig& config, const ChannelConfig& channel_cfg)
    : socket_(socket),
      receiver_ep_(receiver_ep),
      config_(config),
      channel_(channel_cfg),
      timeout_calc_(config.initial_timeout_ms, config.min_timeout_ms, config.max_timeout_ms) {}

Frame StopAndWaitSender::Framing(uint8_t seq, const vector<uint8_t>& data) {
    return Frame::create_data(seq, data, config_.fcs_method);
}

ChannelAction StopAndWaitSender::Channel(vector<uint8_t>& wire_bytes, const Frame& f) {
    return channel_.process_outgoing(wire_bytes, f);
}

bool StopAndWaitSender::Send(const Frame& f) {
    vector<uint8_t> wire_bytes = f.serialize();
    stats_.total_wire_bytes_sent += wire_bytes.size();
    stats_.total_transmissions++;

    ChannelAction action = channel_.transmit(socket_, receiver_ep_, wire_bytes, f);
    if (action == ChannelAction::DROP) {
        stats_.dropped_frames++;
        return true;
    }
    if (action == ChannelAction::CORRUPT) {
        stats_.corrupted_frames++;
    }
    return true;
}

bool StopAndWaitSender::Timer(double timeout_ms) {
    return timer_.is_expired(timeout_ms);
}

void StopAndWaitSender::Timeout(double sample_rtt) {
    timeout_calc_.update_rtt(sample_rtt);
}

bool StopAndWaitSender::Recv(Frame& ack_frame, double timeout_ms) {
    Endpoint src;
    vector<uint8_t> buffer;
    int wait_ms = static_cast<int>(timeout_ms);
    if (wait_ms < 1) wait_ms = 1;

    ssize_t bytes = socket_.recv_from(buffer, src, wait_ms);
    if (bytes <= 0) {
        return false;
    }

    if (!Frame::deserialize(buffer.data(), bytes, ack_frame)) {
        return false;
    }

    if (!ack_frame.verify_fcs(config_.fcs_method)) {
        stats_.corrupted_frames++;
        if (config_.verbose) {
            cout << "  [SENDER] Corrupted ACK frame received, ignoring.\n";
        }
        return false;
    }

    return true;
}

bool StopAndWaitSender::send_file(const string& input_filepath) {
    ifstream file(input_filepath, ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Cannot open input file " << input_filepath << "\n";
        return false;
    }

    auto start_time = Clock::now();
    uint8_t seq = 0;
    vector<uint8_t> chunk(config_.payload_size);
    double total_rtt_samples = 0.0;
    uint64_t rtt_count = 0;

    cout << "\n>>> [STOP-AND-WAIT] Starting file transmission: " << input_filepath << " <<<\n";

    while (file.read(reinterpret_cast<char*>(chunk.data()), config_.payload_size) || file.gcount() > 0) {
        size_t bytes_read = file.gcount();
        vector<uint8_t> payload_data(chunk.begin(), chunk.begin() + bytes_read);

        Frame frame = Framing(seq, payload_data);
        stats_.original_data_frames++;
        stats_.payload_bytes_delivered += bytes_read;

        bool acked = false;
        bool retransmitted = false;

        while (!acked) {
            if (config_.verbose) {
                string preview(reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size());
                cout << (retransmitted ? "[SENDER -> RETRANSMIT DATA] " : "[SENDER -> TX DATA] ")
                     << frame.to_string() << "\n"
                     << "  | Text: \"" << preview << "\"\n";
            }

            auto send_tp = Clock::now();
            timer_.start();
            Send(frame);

            while (!timer_.is_expired(timeout_calc_.get_rto_ms())) {
                double remaining_ms = timeout_calc_.get_rto_ms() - timer_.elapsed_ms();
                if (remaining_ms < 1.0) remaining_ms = 1.0;

                Frame ack_pkt;
                if (Recv(ack_pkt, remaining_ms)) {
                    if (ack_pkt.header.type == static_cast<uint8_t>(FrameType::ACK) &&
                        ack_pkt.header.seq_num == seq) {
                        acked = true;
                        stats_.acks_sent_or_received++;
                        timer_.stop();

                        auto ack_tp = Clock::now();
                        chrono::duration<double, milli> rtt_dur = ack_tp - send_tp;
                        double sample_rtt = rtt_dur.count();

                        Timeout(sample_rtt);
                        total_rtt_samples += sample_rtt;
                        rtt_count++;

                        if (config_.verbose) {
                            cout << "  [SENDER <- RX ACK] Received " << ack_pkt.to_string()
                                 << " | Sample RTT: " << sample_rtt << " ms"
                                 << " | New RTO: " << timeout_calc_.get_rto_ms() << " ms\n";
                        }
                        break;
                    }
                }
            }

            if (!acked) {
                stats_.timeouts++;
                stats_.retransmissions++;
                retransmitted = true;
                if (config_.verbose) {
                    cout << "  [SENDER TIMEOUT] Frame Seq=" << static_cast<int>(seq)
                         << " timed out! Retransmitting... | RTO=" << timeout_calc_.get_rto_ms() << " ms\n";
                }
            }
        }

        seq = 1 - seq; // toggle sequence number for stop-and-wait
    }

    // send fin to notify receiver of eof
    Frame fin = Frame::create_fin(seq, config_.fcs_method);
    for (int i = 0; i < 5; i++) {
        Send(fin);
        Frame ack;
        if (Recv(ack, 100.0) && ack.header.type == static_cast<uint8_t>(FrameType::ACK)) {
            break;
        }
    }

    auto end_time = Clock::now();
    chrono::duration<double, milli> total_dur = end_time - start_time;
    stats_.elapsed_time_ms = total_dur.count();
    stats_.avg_rtt_ms = (rtt_count > 0) ? (total_rtt_samples / rtt_count) : 0.0;
    stats_.final_rto_ms = timeout_calc_.get_rto_ms();

    cout << ">>> [STOP-AND-WAIT] Transmission Complete! <<<\n";
    return true;
}

StopAndWaitReceiver::StopAndWaitReceiver(UdpSocket& socket, const ProtocolConfig& config,
                                         const ChannelConfig& ack_channel_cfg)
    : socket_(socket),
      config_(config),
      channel_(ack_channel_cfg) {}

bool StopAndWaitReceiver::Recv(Frame& frame, Endpoint& sender_ep, int timeout_ms) {
    vector<uint8_t> buffer;
    ssize_t bytes = socket_.recv_from(buffer, sender_ep, timeout_ms);
    if (bytes <= 0) return false;
    return Frame::deserialize(buffer.data(), bytes, frame);
}

bool StopAndWaitReceiver::Check(const Frame& frame) {
    return frame.verify_fcs(config_.fcs_method);
}

bool StopAndWaitReceiver::Send(const Frame& ack_frame, const Endpoint& sender_ep) {
    vector<uint8_t> wire = ack_frame.serialize();
    stats_.total_wire_bytes_sent += wire.size();

    ChannelAction action = channel_.process_outgoing(wire, ack_frame);
    if (action == ChannelAction::DROP) {
        stats_.dropped_frames++;
        return true;
    }
    if (action == ChannelAction::CORRUPT) {
        stats_.corrupted_frames++;
    }

    ssize_t sent = socket_.send_to(wire, sender_ep);
    if (sent > 0) {
        stats_.acks_sent_or_received++;
        return true;
    }
    return false;
}

bool StopAndWaitReceiver::receive_file(const string& output_filepath) {
    ofstream outfile(output_filepath, ios::binary | ios::trunc);
    if (!outfile.is_open()) {
        cerr << "Error: Cannot open output file " << output_filepath << "\n";
        return false;
    }

    cout << "\n>>> [STOP-AND-WAIT RECEIVER] Listening on port "
         << socket_.get_bound_port() << " ... <<<\n";

    uint8_t expected_seq = 0;
    Endpoint sender_ep;

    while (true) {
        Frame frame;
        if (!Recv(frame, sender_ep, -1)) {
            continue;
        }

        stats_.total_transmissions++;

        if (!Check(frame)) {
            stats_.corrupted_frames++;
            if (config_.verbose) {
                cout << "  [RECEIVER <- CORRUPTED FRAME] FCS mismatch on Frame Seq="
                     << static_cast<int>(frame.header.seq_num) << "! Discarding.\n";
            }
            continue;
        }

        if (frame.header.type == static_cast<uint8_t>(FrameType::FIN)) {
            if (config_.verbose) {
                cout << "  [RECEIVER <- RX FIN] End of file signal received. Transmission ended.\n";
            }
            Frame fin_ack = Frame::create_ack(frame.header.seq_num, config_.fcs_method);
            Send(fin_ack, sender_ep);
            break;
        }

        if (frame.header.type == static_cast<uint8_t>(FrameType::DATA)) {
            if (frame.header.seq_num == expected_seq) {
                outfile.write(reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size());
                outfile.flush();
                stats_.frames_accepted++;
                stats_.payload_bytes_delivered += frame.payload.size();

                if (config_.verbose) {
                    string preview(reinterpret_cast<const char*>(frame.payload.data()), frame.payload.size());
                    cout << "[RECEIVER <- RX DATA] Accepted " << frame.to_string() << "\n"
                         << "  | Live Data: \"" << preview << "\"\n"
                         << "  -> [RECEIVER -> TX ACK] Sent ACK " << static_cast<int>(expected_seq) << "\n";
                }

                Frame ack = Frame::create_ack(expected_seq, config_.fcs_method);
                Send(ack, sender_ep);
                expected_seq = 1 - expected_seq;
            } else {
                // duplicate frame: resend ack for received sequence number
                stats_.duplicate_frames++;
                if (config_.verbose) {
                    cout << "[RECEIVER <- RX DUPLICATE] Duplicate " << frame.to_string()
                         << " already processed -> [RECEIVER -> TX ACK] Resending ACK "
                         << static_cast<int>(frame.header.seq_num) << "\n";
                }
                Frame ack = Frame::create_ack(frame.header.seq_num, config_.fcs_method);
                Send(ack, sender_ep);
            }
        }
    }

    cout << ">>> [STOP-AND-WAIT RECEIVER] Finished receiving. Saved to "
         << output_filepath << " <<<\n";
    return true;
}

} // namespace net
