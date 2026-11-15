#ifndef FTP_SERVER_HPP
#define FTP_SERVER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include "common.hpp"
#include "sandbox_fs.hpp"
#include "ftp_session.hpp"

namespace ftp {

class FtpServer {
public:
    FtpServer(std::string bind_ip, uint16_t port, std::string root_dir);
    ~FtpServer();

    // User management
    void add_user(const std::string& username, const std::string& password, bool read_only = false);
    void set_allow_anonymous(bool allow, bool read_only = true);

    // Start server (runs listener thread or blocks)
    bool start();
    void stop();
    bool is_running() const { return is_running_.load(); }

    uint16_t port() const { return port_; }
    const std::string& bind_ip() const { return bind_ip_; }
    const std::string& root_dir() const { return fs_->root_dir(); }

private:
    std::string bind_ip_;
    uint16_t port_;
    std::shared_ptr<SandboxFs> fs_;

    int server_fd_{-1};
    std::atomic<bool> is_running_{false};

    // User accounts
    std::unordered_map<std::string, UserCredentials> users_;
    bool allow_anonymous_{true};
    bool anonymous_read_only_{true};
    std::mutex users_mutex_;

    // Thread management
    std::thread listener_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex threads_mutex_;
    std::atomic<uint64_t> session_counter_{0};

    void listener_loop();
    bool validate_user(const std::string& username, const std::string& password, bool& read_only);
};

} // namespace ftp

#endif // FTP_SERVER_HPP
