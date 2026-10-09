#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <iomanip>

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
         << "  -f, --file <filepath>          Input file path to transmit\n"
         << "  -i, --ip <ip_address>          Receiver IP address (default: 127.0.0.1)\n"
         << "  -P, --port <port>              Receiver UDP port (default: 9876)\n"
         << "  -w, --window <N>               Window size N (for GBN and SR, default: 4)\n"
         << "  -s, --payload <bytes>          Payload size per frame (46-1500, default: 64)\n"
         << "  -l, --loss <prob>              Frame loss probability 0.0-1.0 (default: 0.0)\n"
         << "  -e, --error <prob>             Bit error probability 0.0-1.0 (default: 0.0)\n"
         << "  -d, --delay <ms>               Max random propagation delay ms (default: 0.0)\n"
         << "  -m, --fcs <0-4>                FCS method: 0=Checksum, 1=CRC8, 2=CRC10, 3=CRC16, 4=CRC32 (default: 4)\n"
         << "  -t, --timeout <ms>             Initial RTO timeout in ms (default: 200.0)\n"
         << "  -q, --quiet                    Suppress verbose per-frame logs\n"
         << "  -h, --help                     Show this help message\n";
}

int main(int argc, char* argv[]) {
    ProtocolConfig proto_cfg;
    ChannelConfig chan_cfg;
    string receiver_ip = "127.0.0.1";
    uint16_t receiver_port = 9876;
    string file_path = "";
    bool interactive = (argc <= 1);

    if (interactive) {
        cout << "========================================================\n"
             << "  CN Lab Assignment 2: Flow Control Protocol Sender     \n"
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

        if (proto_cfg.protocol != ProtocolType::STOP_AND_WAIT) {
            cout << "Enter window size N [default 4]: ";
            string w_str;
            getline(cin, w_str);
            if (!w_str.empty()) proto_cfg.window_size = stoi(w_str);
        }

        cout << "Enter input file path [default test_input.txt]: ";
        getline(cin, file_path);
        if (file_path.empty()) file_path = "test_input.txt";

        cout << "Enter Receiver IP [default 127.0.0.1]: ";
        string ip_str;
        getline(cin, ip_str);
        if (!ip_str.empty()) receiver_ip = ip_str;

        cout << "Enter Receiver Port [default 9876]: ";
        string port_str;
        getline(cin, port_str);
        if (!port_str.empty()) receiver_port = stoi(port_str);

        cout << "Enter simulated packet loss probability (0.0 - 0.5) [default 0.0]: ";
        string loss_str;
        getline(cin, loss_str);
        if (!loss_str.empty()) chan_cfg.loss_prob = stod(loss_str);

        cout << "Enter simulated bit error probability (0.0 - 0.5) [default 0.0]: ";
        string err_str;
        getline(cin, err_str);
        if (!err_str.empty()) chan_cfg.error_prob = stod(err_str);

        cout << "Enter simulated random propagation delay in ms [default 0]: ";
        string del_str;
        getline(cin, del_str);
        if (!del_str.empty()) {
            chan_cfg.max_delay_ms = stod(del_str);
            chan_cfg.min_delay_ms = chan_cfg.max_delay_ms / 2.0;
        }

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
            } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
                file_path = argv[++i];
            } else if ((arg == "-i" || arg == "--ip") && i + 1 < argc) {
                receiver_ip = argv[++i];
            } else if ((arg == "-P" || arg == "--port") && i + 1 < argc) {
                receiver_port = stoi(argv[++i]);
            } else if ((arg == "-w" || arg == "--window") && i + 1 < argc) {
                proto_cfg.window_size = stoi(argv[++i]);
            } else if ((arg == "-s" || arg == "--payload") && i + 1 < argc) {
                proto_cfg.payload_size = stoul(argv[++i]);
            } else if ((arg == "-l" || arg == "--loss") && i + 1 < argc) {
                chan_cfg.loss_prob = stod(argv[++i]);
            } else if ((arg == "-e" || arg == "--error") && i + 1 < argc) {
                chan_cfg.error_prob = stod(argv[++i]);
            } else if ((arg == "-d" || arg == "--delay") && i + 1 < argc) {
                chan_cfg.max_delay_ms = stod(argv[++i]);
                chan_cfg.min_delay_ms = chan_cfg.max_delay_ms / 2.0;
            } else if ((arg == "-m" || arg == "--fcs") && i + 1 < argc) {
                proto_cfg.fcs_method = static_cast<ErrorDetectionMethod>(stoi(argv[++i]));
            } else if ((arg == "-t" || arg == "--timeout") && i + 1 < argc) {
                proto_cfg.initial_timeout_ms = stod(argv[++i]);
            } else if (arg == "-q" || arg == "--quiet") {
                proto_cfg.verbose = false;
            } else if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
        }
    }

    if (file_path.empty()) {
        cerr << "Error: Input file must be specified (-f/--file)\n";
        print_usage(argv[0]);
        return 1;
    }

    UdpSocket socket;
    Endpoint receiver_ep(receiver_ip, receiver_port);

    // stop-and-wait sender window size is always 1
    if (proto_cfg.protocol == ProtocolType::STOP_AND_WAIT) {
        proto_cfg.window_size = 1;
    }

    cout << "\nSender Configuration:\n"
         << "  Protocol:    " << protocol_type_to_string(proto_cfg.protocol) << "\n";
    if (proto_cfg.protocol == ProtocolType::STOP_AND_WAIT) {
        cout << "  Window Size: 1 (Stop-and-Wait Sender Window = 1)\n";
    } else {
        cout << "  Window Size: " << proto_cfg.window_size << " (Sender Window = N)\n";
    }
    cout << "  Payload:     " << proto_cfg.payload_size << " bytes\n"
         << "  Receiver:    " << receiver_ep.to_string() << "\n"
         << "  Loss Prob:   " << chan_cfg.loss_prob << "\n"
         << "  Error Prob:  " << chan_cfg.error_prob << "\n"
         << "  Delay Range: [" << chan_cfg.min_delay_ms << ", " << chan_cfg.max_delay_ms << "] ms\n";

    switch (proto_cfg.protocol) {
        case ProtocolType::STOP_AND_WAIT: {
            StopAndWaitSender sender(socket, receiver_ep, proto_cfg, chan_cfg);
            if (!sender.send_file(file_path)) return 1;
            sender.get_stats().print();
            break;
        }
        case ProtocolType::GO_BACK_N: {
            GoBackNSender sender(socket, receiver_ep, proto_cfg, chan_cfg);
            if (!sender.send_file(file_path)) return 1;
            sender.get_stats().print();
            break;
        }
        case ProtocolType::SELECTIVE_REPEAT: {
            SelectiveRepeatSender sender(socket, receiver_ep, proto_cfg, chan_cfg);
            if (!sender.send_file(file_path)) return 1;
            sender.get_stats().print();
            break;
        }
    }

    return 0;
}
