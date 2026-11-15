#include "ftp_server.hpp"
#include "logger.hpp"
#include "socket_utils.hpp"
#include <iostream>

namespace ftp {

FtpServer::FtpServer(std::string bind_ip, uint16_t port, std::string root_dir)
    : bind_ip_(std::move(bind_ip)),
      port_(port),
      fs_(std::make_shared<SandboxFs>(std::move(root_dir))) {
    // Register default out-of-the-box accounts
    add_user("admin", "admin123", false);   // Full Read-Write administrator
    add_user("student", "cnlab", false);    // Laboratory student account
    add_user("guest", "guest", true);       // Read-Only guest account
    set_allow_anonymous(true, true);        // Anonymous read-only enabled by default
}

FtpServer::~FtpServer() {
    stop();
}

void FtpServer::add_user(const std::string& username, const std::string& password, bool read_only) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    users_[username] = UserCredentials{username, password, read_only};
}

void FtpServer::set_allow_anonymous(bool allow, bool read_only) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    allow_anonymous_ = allow;
    anonymous_read_only_ = read_only;
}

bool FtpServer::validate_user(const std::string& username, const std::string& password, bool& read_only) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    if (username == "anonymous") {
        if (allow_anonymous_) {
            read_only = anonymous_read_only_;
            return true;
        }
        return false;
    }

    auto it = users_.find(username);
    if (it != users_.end()) {
        if (it->second.password == password) {
            read_only = it->second.read_only;
            return true;
        }
    }
    return false;
}

bool FtpServer::start() {
    if (is_running_) return true;

    server_fd_ = SocketUtils::create_tcp_listener(bind_ip_, port_, 32);
    if (server_fd_ < 0) {
        Logger::error("SERVER", "Failed to bind listening socket on " + bind_ip_ + ":" + std::to_string(port_));
        return false;
    }

    // If ephemeral port 0 was passed, query assigned port
    if (port_ == 0) {
        std::string actual_ip;
        SocketUtils::get_socket_address(server_fd_, actual_ip, port_);
    }

    is_running_ = true;
    Logger::success("SERVER", "FTP Server listening on " + bind_ip_ + ":" + std::to_string(port_) +
                             " [Root: " + fs_->root_dir() + "]");

    listener_thread_ = std::thread(&FtpServer::listener_loop, this);
    return true;
}

void FtpServer::stop() {
    if (!is_running_) return;

    Logger::info("SERVER", "Stopping FTP Server on port " + std::to_string(port_) + "...");
    is_running_ = false;

    if (server_fd_ >= 0) {
        SocketUtils::close_socket(server_fd_);
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }

    std::lock_guard<std::mutex> lock(threads_mutex_);
    for (auto& t : client_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    client_threads_.clear();
    Logger::success("SERVER", "FTP Server halted cleanly.");
}

void FtpServer::listener_loop() {
    while (is_running_) {
        std::string client_ip;
        uint16_t client_port = 0;
        int client_fd = SocketUtils::accept_client(server_fd_, client_ip, client_port);

        if (client_fd < 0) {
            if (!is_running_) break;
            continue;
        }

        uint64_t sid = ++session_counter_;
        std::string session_id = "#" + std::to_string(sid);

        std::lock_guard<std::mutex> lock(threads_mutex_);
        client_threads_.emplace_back([this, client_fd, client_ip, client_port, session_id]() {
            auto validator = [this](const std::string& u, const std::string& p, bool& ro) {
                return this->validate_user(u, p, ro);
            };

            FtpSession session(client_fd, client_ip, client_port, session_id, fs_, validator);
            session.run();
        });
    }
}

} // namespace ftp
