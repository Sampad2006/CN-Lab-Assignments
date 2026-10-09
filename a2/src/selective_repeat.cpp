#include "selective_repeat.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

using namespace std;

namespace net {

SelectiveRepeatSender::SelectiveRepeatSender(UdpSocket& socket, const Endpoint& receiver_ep,
                                             const ProtocolConfig& config, const ChannelConfig& channel_cfg)
    : socket_(socket),
      receiver_ep_(receiver_ep),
      config_(config),
      channel_(channel_cfg),
      timeout_calc_(config.initial_timeout_ms, config.min_timeout_ms, config.max_timeout_ms),
      seq_mod_(256) {}

Frame SelectiveRepeatSender::Framing(uint8_t seq, const vector<uint8_t>& data) {
    return Frame::create_data(seq, data, config_.fcs_method);
}

ChannelAction SelectiveRepeatSender::Channel(vector<uint8_t>& wire_bytes, const Frame& f) {
    return channel_.process_outgoing(wire_bytes, f);
}

bool SelectiveRepeatSender::Send(const Frame& f) {
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

bool SelectiveRepeatSender::Timer(double timeout_ms) {
    (void)timeout_ms;
    return false;
}

void SelectiveRepeatSender::Timeout(double sample_rtt) {
    timeout_calc_.update_rtt(sample_rtt);
}

bool SelectiveRepeatSender::Recv(Frame& ack_frame, double timeout_ms) {
    Endpoint src;
    vector<uint8_t> buffer;
    int wait_ms = static_cast<int>(timeout_ms);
    if (wait_ms < 1) wait_ms = 1;

    ssize_t bytes = socket_.recv_from(buffer, src, wait_ms);
    if (bytes <= 0) return false;

    if (!Frame::deserialize(buffer.data(), bytes, ack_frame)) return false;

    if (!ack_frame.verify_fcs(config_.fcs_method)) {
        stats_.corrupted_frames++;
        if (config_.verbose) {
            cout << "  [SR SENDER] Corrupted ACK/NAK frame, discarded.\n";
        }
        return false;
    }

    return true;
}

bool SelectiveRepeatSender::send_file(const string& input_filepath) {
    ifstream file(input_filepath, ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Cannot open input file " << input_filepath << "\n";
        return false;
    }

    // read all chunks
    vector<vector<uint8_t>> chunks;
    vector<uint8_t> buffer(config_.payload_size);
    while (file.read(reinterpret_cast<char*>(buffer.data()), config_.payload_size) || file.gcount() > 0) {
        size_t bytes = file.gcount();
        chunks.emplace_back(buffer.begin(), buffer.begin() + bytes);
        stats_.payload_bytes_delivered += bytes;
    }

    uint64_t total_frames = chunks.size();
    stats_.original_data_frames = total_frames;

    cout << "\n>>> [SELECTIVE REPEAT] Transmitting " << total_frames << " frames from "
         << input_filepath << " (Window Size N=" << config_.window_size << ") <<<\n";

    auto start_time = Clock::now();
    uint64_t base = 0;

    vector<SrPacketInfo> packets(total_frames);
    for (uint64_t i = 0; i < total_frames; i++) {
        packets[i].abs_index = i;
        packets[i].seq_num = static_cast<uint8_t>(i % seq_mod_);
        packets[i].frame = Framing(packets[i].seq_num, chunks[i]);
        packets[i].status = PacketStatus::UNSENT;
        packets[i].retransmitted = false;
    }

    double total_rtt_samples = 0.0;
    uint64_t rtt_count = 0;

    while (base < total_frames) {
        // send unsent packets within window [base, base + N - 1]
        uint64_t win_end = min(base + static_cast<uint64_t>(config_.window_size), total_frames);
        for (uint64_t i = base; i < win_end; i++) {
            if (packets[i].status == PacketStatus::UNSENT) {
                if (config_.verbose) {
                    string preview(reinterpret_cast<const char*>(packets[i].frame.payload.data()),
                                   packets[i].frame.payload.size());
                    cout << "[SR SENDER -> TX DATA] Frame idx=" << i
                         << " " << packets[i].frame.to_string() << "\n"
                         << "  | Text: \"" << preview << "\"\n";
                }
                packets[i].status = PacketStatus::SENT;
                packets[i].send_time = Clock::now();
                packets[i].timer.start();
                Send(packets[i].frame);
            }
        }

        // compute shortest remaining timeout among active frames
        double min_remaining = timeout_calc_.get_rto_ms();
        bool has_active_timer = false;
        for (uint64_t i = base; i < win_end; i++) {
            if (packets[i].status == PacketStatus::SENT && packets[i].timer.is_running()) {
                has_active_timer = true;
                double rem = timeout_calc_.get_rto_ms() - packets[i].timer.elapsed_ms();
                if (rem < min_remaining) {
                    min_remaining = rem;
                }
            }
        }
        if (!has_active_timer || min_remaining < 1.0) {
            min_remaining = 1.0;
        }

        // receive acks or naks
        Frame ack_pkt;
        if (Recv(ack_pkt, min_remaining)) {
            uint8_t seq = ack_pkt.header.seq_num;
            if (ack_pkt.header.type == static_cast<uint8_t>(FrameType::ACK)) {
                stats_.acks_sent_or_received++;
                for (uint64_t i = base; i < win_end; i++) {
                    if (packets[i].seq_num == seq && packets[i].status == PacketStatus::SENT) {
                        packets[i].status = PacketStatus::ACKED;
                        packets[i].timer.stop();

                        auto ack_tp = Clock::now();
                        chrono::duration<double, milli> rtt_dur = ack_tp - packets[i].send_time;
                        double sample_rtt = rtt_dur.count();
                        Timeout(sample_rtt);
                        total_rtt_samples += sample_rtt;
                        rtt_count++;

                        if (config_.verbose) {
                            cout << "  [SR SENDER <- RX INDIVIDUAL ACK] Received ACK for Frame idx=" << i
                                 << " (seq=" << static_cast<int>(seq) << ") | New RTO: "
                                 << timeout_calc_.get_rto_ms() << " ms\n";
                        }
                        break;
                    }
                }

                // advance window base past consecutively acked frames
                while (base < total_frames && packets[base].status == PacketStatus::ACKED) {
                    base++;
                }
            } else if (ack_pkt.header.type == static_cast<uint8_t>(FrameType::NAK)) {
                stats_.naks_sent_or_received++;
                // immediate selective retransmission on nak
                for (uint64_t i = base; i < win_end; i++) {
                    if (packets[i].seq_num == seq && packets[i].status == PacketStatus::SENT) {
                        stats_.retransmissions++;
                        packets[i].retransmitted = true;
                        packets[i].send_time = Clock::now();
                        packets[i].timer.reset();
                        if (config_.verbose) {
                            cout << "  [SR SENDER <- RX NAK] Received NAK for Frame idx=" << i
                                 << " (seq=" << static_cast<int>(seq) << ") -> [SR SENDER -> RETRANSMIT DATA]\n";
                        }
                        Send(packets[i].frame);
                        break;
                    }
                }
            }
        }

        // check for per-packet timeouts in the window
        win_end = min(base + static_cast<uint64_t>(config_.window_size), total_frames);
        for (uint64_t i = base; i < win_end; i++) {
            if (packets[i].status == PacketStatus::SENT &&
                packets[i].timer.is_expired(timeout_calc_.get_rto_ms())) {
                stats_.timeouts++;
                stats_.retransmissions++;

                packets[i].retransmitted = true;
                packets[i].send_time = Clock::now();
                packets[i].timer.reset();

                if (config_.verbose) {
                    cout << "  [SR SENDER TIMEOUT] Frame idx=" << i
                         << " (seq=" << static_cast<int>(packets[i].seq_num)
                         << ") timed out! -> [SR SENDER -> RETRANSMIT DATA] | RTO="
                         << timeout_calc_.get_rto_ms() << " ms\n";
                }
                Send(packets[i].frame);
            }
        }
    }

    // send fin frame
    Frame fin = Frame::create_fin(static_cast<uint8_t>(total_frames % seq_mod_), config_.fcs_method);
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

    cout << ">>> [SELECTIVE REPEAT] Transmission Complete! <<<\n";
    return true;
}

SelectiveRepeatReceiver::SelectiveRepeatReceiver(UdpSocket& socket, const ProtocolConfig& config,
                                                 const ChannelConfig& ack_channel_cfg)
    : socket_(socket),
      config_(config),
      channel_(ack_channel_cfg),
      seq_mod_(256) {}

bool SelectiveRepeatReceiver::Recv(Frame& frame, Endpoint& sender_ep, int timeout_ms) {
    vector<uint8_t> buffer;
    ssize_t bytes = socket_.recv_from(buffer, sender_ep, timeout_ms);
    if (bytes <= 0) return false;
    return Frame::deserialize(buffer.data(), bytes, frame);
}

bool SelectiveRepeatReceiver::Check(const Frame& frame) {
    return frame.verify_fcs(config_.fcs_method);
}

bool SelectiveRepeatReceiver::Send(const Frame& ack_frame, const Endpoint& sender_ep) {
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

bool SelectiveRepeatReceiver::receive_file(const string& output_filepath) {
    ofstream outfile(output_filepath, ios::binary | ios::trunc);
    if (!outfile.is_open()) {
        cerr << "Error: Cannot open output file " << output_filepath << "\n";
        return false;
    }

    cout << "\n>>> [SELECTIVE REPEAT RECEIVER] Listening on port "
         << socket_.get_bound_port() << " (Window Size N=" << config_.window_size << ") ... <<<\n";

    uint64_t rcv_base = 0;
    map<uint64_t, Frame> rcv_buffer;
    Endpoint sender_ep;

    while (true) {
        Frame frame;
        if (!Recv(frame, sender_ep, -1)) continue;

        stats_.total_transmissions++;

        if (!Check(frame)) {
            stats_.corrupted_frames++;
            if (config_.verbose) {
                cout << "  [SR RECEIVER <- CORRUPTED FRAME] FCS mismatch on Seq="
                     << static_cast<int>(frame.header.seq_num)
                     << "! -> [SR RECEIVER -> TX NAK] Sent NAK " << static_cast<int>(frame.header.seq_num) << "\n";
            }
            Frame nak = Frame::create_nak(frame.header.seq_num, config_.fcs_method);
            Send(nak, sender_ep);
            continue;
        }

        if (frame.header.type == static_cast<uint8_t>(FrameType::FIN)) {
            if (config_.verbose) {
                cout << "  [SR RECEIVER <- RX FIN] End of file signal received. Transmission complete.\n";
            }
            Frame fin_ack = Frame::create_ack(frame.header.seq_num, config_.fcs_method);
            Send(fin_ack, sender_ep);
            break;
        }

        if (frame.header.type == static_cast<uint8_t>(FrameType::DATA)) {
            uint8_t seq = frame.header.seq_num;

            int64_t low_bound = (static_cast<int64_t>(rcv_base) >= config_.window_size) ?
                                (static_cast<int64_t>(rcv_base) - config_.window_size) : 0;
            int64_t high_bound = static_cast<int64_t>(rcv_base) + config_.window_size - 1;

            int64_t matched_idx = -1;
            for (int64_t cand = low_bound; cand <= high_bound; cand++) {
                if (static_cast<uint8_t>(cand % seq_mod_) == seq) {
                    matched_idx = cand;
                    break;
                }
            }

            if (matched_idx >= static_cast<int64_t>(rcv_base) &&
                matched_idx < static_cast<int64_t>(rcv_base) + config_.window_size) {
                uint64_t abs_idx = static_cast<uint64_t>(matched_idx);

                if (config_.verbose) {
                    cout << "[SR RECEIVER <- RX DATA] Accepted Frame idx=" << abs_idx
                         << " " << frame.to_string() << " -> [SR RECEIVER -> TX ACK] Sent Independent ACK "
                         << static_cast<int>(seq) << "\n";
                }

                // buffer packet and send independent ack
                rcv_buffer[abs_idx] = frame;
                Frame ack = Frame::create_ack(seq, config_.fcs_method);
                Send(ack, sender_ep);

                // deliver contiguous in-order frames
                while (rcv_buffer.count(rcv_base) > 0) {
                    const Frame& f = rcv_buffer[rcv_base];
                    outfile.write(reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
                    outfile.flush();
                    stats_.frames_accepted++;
                    stats_.payload_bytes_delivered += f.payload.size();

                    if (config_.verbose) {
                        string preview(reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
                        cout << "  [SR RECEIVER DELIVER] In-Order Frame idx=" << rcv_base
                             << " written to file.\n"
                             << "  | Live Data: \"" << preview << "\"\n";
                    }

                    rcv_buffer.erase(rcv_base);
                    rcv_base++;
                }
            } else if (matched_idx >= low_bound && matched_idx < static_cast<int64_t>(rcv_base)) {
                // duplicate frame: resend ack to prevent sender deadlock
                stats_.duplicate_frames++;
                if (config_.verbose) {
                    cout << "[SR RECEIVER <- RX DUPLICATE] Frame idx=" << matched_idx
                         << " (seq=" << static_cast<int>(seq)
                         << ") already accepted -> [SR RECEIVER -> TX ACK] Resending ACK\n";
                }
                Frame ack = Frame::create_ack(seq, config_.fcs_method);
                Send(ack, sender_ep);
            } else {
                if (config_.verbose) {
                    cout << "[SR RECEIVER OUT-OF-WINDOW] Frame (seq=" << static_cast<int>(seq)
                         << ") outside window, discarded.\n";
                }
            }
        }
    }

    cout << ">>> [SELECTIVE REPEAT RECEIVER] Finished receiving. Saved to "
         << output_filepath << " <<<\n";
    return true;
}

} // namespace net
