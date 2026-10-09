#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

#include "protocol.hpp"
#include "stop_and_wait.hpp"
#include "go_back_n.hpp"
#include "selective_repeat.hpp"
#include "socket.hpp"
#include "channel.hpp"

using namespace std;
using namespace net;

void print_usage(const char* prog) {
    cout << "Usage: " << prog << " [options]\n\n"
         << "Options:\n"
         << "  -p, --protocol <sw|gbn|sr>     Flow control protocol (default: sw)\n"
         << "  -o, --output <filepath>        Output file path (default: received.txt)\n"
         << "  -P, --port <port>              Listening UDP port (default: 9876)\n"
         << "  -w, --window <N>               Window size N (for SR, default: 4)\n"
         << "  -m, --fcs <0-4>                FCS method: 0=Checksum, 1=CRC8, 2=CRC10, 3=CRC16, 4=CRC32 (default: 4)\n"
         << "  -l, --loss <prob>              ACK loss probability 0.0-1.0 (default: 0.0)\n"
         << "  -q, --quiet                    Suppress verbose per-frame logs\n"
         << "  -h, --help                     Show this help message\n";
}

int main(int argc, char* argv[]) {
    ProtocolConfig proto_cfg;
    ChannelConfig ack_chan_cfg;
    uint16_t listen_port = 9876;
    string output_path = "received.txt";
    bool interactive = (argc <= 1);

    if (interactive) {
        cout << "========================================================\n"
             << "  CN Lab Assignment 2: Flow Control Protocol Receiver   \n"
             << "========================================================\n";

        cout << "Select Flow Control Protocol:\n"
             << "  1. Stop-and-Wait ARQ\n"
             << "  2. Go-Back-N ARQ\n"
             << "  3. Selective Repeat ARQ\n"
             << "Enter choice (1-3) [default 1]: ";
        string p_choice;
        getline(cin, p_choice);
        if (p_choice == "2") proto_cfg.protocol = ProtocolType::GO_BACK_N;
        else if (p_choice == "3") proto_cfg.protocol = ProtocolType::SELECTIVE_REPEAT;
        else proto_cfg.protocol = ProtocolType::STOP_AND_WAIT;

        if (proto_cfg.protocol == ProtocolType::SELECTIVE_REPEAT) {
            cout << "Enter receiver window size N [default 4]: ";
            string w_str;
            getline(cin, w_str);
            if (!w_str.empty()) proto_cfg.window_size = stoi(w_str);
        }

        cout << "Enter output file path [default received.txt]: ";
        getline(cin, output_path);
        if (output_path.empty()) output_path = "received.txt";

        cout << "Enter Listening UDP Port [default 9876]: ";
        string port_str;
        getline(cin, port_str);
        if (!port_str.empty()) listen_port = stoi(port_str);

        cout << "Select Error Detection Method (0: Checksum, 1: CRC8, 2: CRC10, 3: CRC16, 4: CRC32) [default 4]: ";
        string fcs_str;
        getline(cin, fcs_str);
        if (!fcs_str.empty()) {
            proto_cfg.fcs_method = static_cast<ErrorDetectionMethod>(stoi(fcs_str));
        }
    } else {
        for (int i = 1; i < argc; i++) {
            string arg = argv[i];
            if ((arg == "-p" || arg == "--protocol") && i + 1 < argc) {
                proto_cfg.protocol = string_to_protocol_type(argv[++i]);
            } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                output_path = argv[++i];
            } else if ((arg == "-P" || arg == "--port") && i + 1 < argc) {
                listen_port = stoi(argv[++i]);
            } else if ((arg == "-w" || arg == "--window") && i + 1 < argc) {
                proto_cfg.window_size = stoi(argv[++i]);
            } else if ((arg == "-m" || arg == "--fcs") && i + 1 < argc) {
                proto_cfg.fcs_method = static_cast<ErrorDetectionMethod>(stoi(argv[++i]));
            } else if ((arg == "-l" || arg == "--loss") && i + 1 < argc) {
                ack_chan_cfg.loss_prob = stod(argv[++i]);
            } else if (arg == "-q" || arg == "--quiet") {
                proto_cfg.verbose = false;
            } else if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
        }
    }

    UdpSocket socket;
    if (!socket.bind(listen_port)) {
        cerr << "Failed to bind socket to port " << listen_port << "\n";
        return 1;
    }

    // stop-and-wait and go-back-n both have a receiver window size of 1
    if (proto_cfg.protocol == ProtocolType::STOP_AND_WAIT ||
        proto_cfg.protocol == ProtocolType::GO_BACK_N) {
        proto_cfg.window_size = 1;
    }

    cout << "\nReceiver Configuration:\n"
         << "  Protocol:    " << protocol_type_to_string(proto_cfg.protocol) << "\n";
    if (proto_cfg.protocol == ProtocolType::STOP_AND_WAIT) {
        cout << "  Window Size: 1 (Stop-and-Wait Receiver Window = 1)\n";
    } else if (proto_cfg.protocol == ProtocolType::GO_BACK_N) {
        cout << "  Window Size: 1 (Go-Back-N Receiver Window = 1, in-order delivery)\n";
    } else {
        cout << "  Window Size: " << proto_cfg.window_size << " (Selective Repeat Receiver Window = N)\n";
    }
    cout << "  Listen Port: " << listen_port << "\n"
         << "  Output File: " << output_path << "\n";

    switch (proto_cfg.protocol) {
        case ProtocolType::STOP_AND_WAIT: {
            StopAndWaitReceiver receiver(socket, proto_cfg, ack_chan_cfg);
            if (!receiver.receive_file(output_path)) return 1;
            receiver.get_stats().print_receiver(output_path);
            break;
        }
        case ProtocolType::GO_BACK_N: {
            GoBackNReceiver receiver(socket, proto_cfg, ack_chan_cfg);
            if (!receiver.receive_file(output_path)) return 1;
            receiver.get_stats().print_receiver(output_path);
            break;
        }
        case ProtocolType::SELECTIVE_REPEAT: {
            SelectiveRepeatReceiver receiver(socket, proto_cfg, ack_chan_cfg);
            if (!receiver.receive_file(output_path)) return 1;
            receiver.get_stats().print_receiver(output_path);
            break;
        }
    }

    return 0;
}
