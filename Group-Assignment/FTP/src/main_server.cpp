#include "ftp_server.hpp"
#include "logger.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_shutdown_requested{false};
static ftp::FtpServer* g_server_instance{nullptr};

static void signal_handler(int sig) {
    (void)sig;
    g_shutdown_requested = true;
    if (g_server_instance) {
        g_server_instance->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string bind_ip = "0.0.0.0";
    uint16_t port = ftp::DEFAULT_FTP_PORT;
    std::string root_dir = "./ftp_root";
    bool allow_anonymous = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-d" && i + 1 < argc) {
            root_dir = argv[++i];
        } else if (arg == "-b" && i + 1 < argc) {
            bind_ip = argv[++i];
        } else if (arg == "--no-anon") {
            allow_anonymous = false;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -p <port>       Port to bind control connection (default: " << ftp::DEFAULT_FTP_PORT << ")\n"
                      << "  -d <dir>        Root directory for chrooted sandbox (default: ./ftp_root)\n"
                      << "  -b <ip>         Bind IP address (default: 0.0.0.0)\n"
                      << "  --no-anon       Disable anonymous access\n"
                      << "  -h, --help      Display this help dialog\n";
            return 0;
        }
    }

    std::cout << ftp::colors::CYAN << ftp::colors::BOLD
              << "====================================================================\n"
              << "       RFC 959 File Transfer Protocol (FTP) Server Daemon           \n"
              << "       CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University   \n"
              << "====================================================================\n"
              << ftp::colors::RESET;

    ftp::FtpServer server(bind_ip, port, root_dir);
    server.set_allow_anonymous(allow_anonymous, true);
    g_server_instance = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    if (!server.start()) {
        ftp::Logger::error("MAIN", "Failed to start FTP server.");
        return 1;
    }

    std::cout << ftp::colors::GREEN << ftp::colors::BOLD
              << "[+] Server running on " << bind_ip << ":" << port << "\n"
              << "[+] Virtual root: " << root_dir << "\n"
              << "[+] Pre-configured accounts:\n"
              << "    - admin    : admin123 (Read-Write)\n"
              << "    - student  : cnlab    (Read-Write)\n"
              << "    - guest    : guest    (Read-Only)\n"
              << "    - anonymous: (empty)  (Read-Only)\n"
              << "[*] Press Ctrl+C to terminate cleanly.\n"
              << ftp::colors::RESET << std::endl;

    while (!g_shutdown_requested && server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\n[+] Server shutdown completed.\n";
    return 0;
}
