#include "go_back_n.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>

using namespace std;

namespace net {

GoBackNSender::GoBackNSender(UdpSocket& socket, const Endpoint& receiver_ep,
                             const ProtocolConfig& config, const ChannelConfig& channel_cfg)
    : socket_(socket),
      receiver_ep_(receiver_ep),
      config_(config),
      channel_(channel_cfg),
      timeout_calc_(config.initial_timeout_ms, config.min_timeout_ms, config.max_timeout_ms),
      seq_mod_(256) {}

Frame GoBackNSender::Framing(uint8_t seq, const vector<uint8_t>& data) {
    return Frame::create_data(seq, data, config_.fcs_method);
}

ChannelAction GoBackNSender::Channel(vector<uint8_t>& wire_bytes, const Frame& f) {
    return channel_.process_outgoing(wire_bytes, f);
}

bool GoBackNSender::Send(const Frame& f) {
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

bool GoBackNSender::Timer(double timeout_ms) {
    return base_timer_.is_expired(timeout_ms);
}

void GoBackNSender::Timeout(double sample_rtt) {
    timeout_calc_.update_rtt(sample_rtt);
}

bool GoBackNSender::Recv(Frame& ack_frame, double timeout_ms) {
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
            cout << "  [GBN SENDER] Corrupted ACK received, discarded.\n";
        }
        return false;
    }

    return true;
}

bool GoBackNSender::send_file(const string& input_filepath) {
    ifstream file(input_filepath, ios::binary);
    if (!file.is_open()) {
        cerr << "Error: Cannot open input file " << input_filepath << "\n";
        return false;
    }

    // read entire file into chunks
    vector<vector<uint8_t>> chunks;
    vector<uint8_t> buffer(config_.payload_size);
    while (file.read(reinterpret_cast<char*>(buffer.data()), config_.payload_size) || file.gcount() > 0) {
        size_t bytes = file.gcount();
        chunks.emplace_back(buffer.begin(), buffer.begin() + bytes);
        stats_.payload_bytes_delivered += bytes;
    }

    uint64_t total_frames = chunks.size();
    stats_.original_data_frames = total_frames;

    cout << "\n>>> [GO-BACK-N] Transmitting " << total_frames << " frames from "
         << input_filepath << " (Window Size N=" << config_.window_size << ") <<<\n";

    auto start_time = Clock::now();
    uint64_t base = 0;
    uint64_t next_seq = 0;

    // track frame metadata
    vector<GbnPacketInfo> packets(total_frames);
    for (uint64_t i = 0; i < total_frames; i++) {
        packets[i].abs_index = i;
        packets[i].seq_num = static_cast<uint8_t>(i % seq_mod_);
        packets[i].frame = Framing(packets[i].seq_num, chunks[i]);
        packets[i].retransmitted = false;
    }

    double total_rtt_samples = 0.0;
    uint64_t rtt_count = 0;

    while (base < total_frames) {
        // transmit packets while window is not full
        while (next_seq < base + config_.window_size && next_seq < total_frames) {
            if (config_.verbose) {
                string preview(reinterpret_cast<const char*>(packets[next_seq].frame.payload.data()),
                               packets[next_seq].frame.payload.size());
                cout << (packets[next_seq].retransmitted ? "[GBN SENDER -> RETRANSMIT DATA] " : "[GBN SENDER -> TX DATA] ")
                     << "Frame idx=" << next_seq << " " << packets[next_seq].frame.to_string() << "\n"
                     << "  | Text: \"" << preview << "\"\n";
            }
            packets[next_seq].send_time = Clock::now();
            Send(packets[next_seq].frame);

            if (base == next_seq) {
                base_timer_.reset();
            }
            next_seq++;
        }

        // wait for ack or timeout
        double remaining_ms = timeout_calc_.get_rto_ms() - base_timer_.elapsed_ms();
        if (remaining_ms < 1.0) remaining_ms = 1.0;

        Frame ack_pkt;
        bool got_ack = Recv(ack_pkt, remaining_ms);

        if (got_ack && ack_pkt.header.type == static_cast<uint8_t>(FrameType::ACK)) {
            uint8_t ack_seq = ack_pkt.header.seq_num;
            stats_.acks_sent_or_received++;

            int match_idx = -1;
            for (uint64_t i = base; i < next_seq; i++) {
                if (packets[i].seq_num == ack_seq) {
                    match_idx = static_cast<int>(i);
                    break;
                }
            }

            if (match_idx >= 0 && static_cast<uint64_t>(match_idx) >= base) {
                // cumulative ack acknowledges packets from base up to match_idx
                uint64_t old_base = base;
                base = match_idx + 1;

                auto ack_tp = Clock::now();
                chrono::duration<double, milli> rtt_dur = ack_tp - packets[match_idx].send_time;
                double sample_rtt = rtt_dur.count();
                Timeout(sample_rtt);
                total_rtt_samples += sample_rtt;
                rtt_count++;

                if (config_.verbose) {
                    cout << "  [GBN SENDER <- RX CUMULATIVE ACK] Received ACK up to Frame idx=" << match_idx
                         << " (ACK seq=" << static_cast<int>(ack_seq)
                         << ") | Window Base: " << old_base << " -> " << base
                         << " | New RTO: " << timeout_calc_.get_rto_ms() << " ms\n";
                }

                if (base == next_seq) {
                    base_timer_.stop();
                } else {
                    base_timer_.reset();
                }
            } else {
                if (config_.verbose) {
                    cout << "  [GBN SENDER <- RX DUPLICATE ACK] Stale/Duplicate ACK seq="
                         << static_cast<int>(ack_seq) << " (Current Base=" << base << "), ignored.\n";
                }
            }
        }

        // check timeout
        if (base_timer_.is_running() && base_timer_.is_expired(timeout_calc_.get_rto_ms())) {
            stats_.timeouts++;

            if (config_.verbose) {
                cout << "  [GBN SENDER TIMEOUT] Base frame idx=" << base
                     << " timed out! Retransmitting window [" << base << ".." << (next_seq - 1)
                     << "] | RTO=" << timeout_calc_.get_rto_ms() << " ms\n";
            }

            // retransmit all packets in current window
            base_timer_.reset();
            for (uint64_t i = base; i < next_seq; i++) {
                packets[i].retransmitted = true;
                packets[i].send_time = Clock::now();
                stats_.retransmissions++;
                if (config_.verbose) {
                    cout << "    [GBN RETRANSMIT] Frame idx=" << i
                         << " " << packets[i].frame.to_string() << "\n";
                }
                Send(packets[i].frame);
            }
        }
    }

    // send fin frame
    Frame fin = Frame::create_fin(static_cast<uint8_t>(next_seq % seq_mod_), config_.fcs_method);
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

    cout << ">>> [GO-BACK-N] Transmission Complete! <<<\n";
    return true;
}

GoBackNReceiver::GoBackNReceiver(UdpSocket& socket, const ProtocolConfig& config,
                                 const ChannelConfig& ack_channel_cfg)
    : socket_(socket),
      config_(config),
      channel_(ack_channel_cfg),
      seq_mod_(256) {}

bool GoBackNReceiver::Recv(Frame& frame, Endpoint& sender_ep, int timeout_ms) {
    vector<uint8_t> buffer;
    ssize_t bytes = socket_.recv_from(buffer, sender_ep, timeout_ms);
    if (bytes <= 0) return false;
    return Frame::deserialize(buffer.data(), bytes, frame);
}

bool GoBackNReceiver::Check(const Frame& frame) {
    return frame.verify_fcs(config_.fcs_method);
}

bool GoBackNReceiver::Send(const Frame& ack_frame, const Endpoint& sender_ep) {
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

bool GoBackNReceiver::receive_file(const string& output_filepath) {
    ofstream outfile(output_filepath, ios::binary | ios::trunc);
    if (!outfile.is_open()) {
        cerr << "Error: Cannot open output file " << output_filepath << "\n";
        return false;
    }

    cout << "\n>>> [GO-BACK-N RECEIVER] Listening on port "
         << socket_.get_bound_port() << " ... <<<\n";

    uint8_t expected_seq = 0;
    int last_acked_seq = -1;
    Endpoint sender_ep;

    while (true) {
        Frame frame;
        if (!Recv(frame, sender_ep, -1)) continue;

        stats_.total_transmissions++;

        if (!Check(frame)) {
            stats_.corrupted_frames++;
            if (config_.verbose) {
                cout << "  [GBN RECEIVER <- CORRUPTED FRAME] FCS mismatch on Frame Seq="
                     << static_cast<int>(frame.header.seq_num) << "! Discarding.\n";
            }
            if (last_acked_seq >= 0) {
                Frame ack = Frame::create_ack(static_cast<uint8_t>(last_acked_seq), config_.fcs_method);
                Send(ack, sender_ep);
            }
            continue;
        }

        if (frame.header.type == static_cast<uint8_t>(FrameType::FIN)) {
            if (config_.verbose) {
                cout << "  [GBN RECEIVER <- RX FIN] End of file signal received. Transmission complete.\n";
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
                    cout << "[GBN RECEIVER <- RX DATA] Accepted Frame Seq=" << static_cast<int>(expected_seq) << "\n"
                         << "  | Live Data: \"" << preview << "\"\n"
                         << "  -> [GBN RECEIVER -> TX ACK] Sent Cumulative ACK " << static_cast<int>(expected_seq) << "\n";
                }

                Frame ack = Frame::create_ack(expected_seq, config_.fcs_method);
                Send(ack, sender_ep);
                last_acked_seq = expected_seq;
                expected_seq = static_cast<uint8_t>((expected_seq + 1) % seq_mod_);
            } else {
                // out of order or duplicate: resend ack for last in-order frame
                stats_.duplicate_frames++;
                if (config_.verbose) {
                    cout << "[GBN RECEIVER <- OUT-OF-ORDER/DUPLICATE] Frame Seq="
                         << static_cast<int>(frame.header.seq_num)
                         << " (Expected " << static_cast<int>(expected_seq)
                         << ") -> [GBN RECEIVER -> TX ACK] Resending Cumulative ACK " << last_acked_seq << "\n";
                }
                if (last_acked_seq >= 0) {
                    Frame ack = Frame::create_ack(static_cast<uint8_t>(last_acked_seq), config_.fcs_method);
                    Send(ack, sender_ep);
                }
            }
        }
    }

    cout << ">>> [GO-BACK-N RECEIVER] Finished receiving. Saved to "
         << output_filepath << " <<<\n";
    return true;
}

} // namespace net
