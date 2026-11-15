#ifndef FTP_SESSION_HPP
#define FTP_SESSION_HPP

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include "common.hpp"
#include "socket_utils.hpp"
#include "ftp_command.hpp"
#include "ftp_reply.hpp"
#include "sandbox_fs.hpp"

namespace ftp {

class FtpServer;

struct UserCredentials {
    std::string username;
    std::string password;
    bool read_only{false};
};

class FtpSession {
public:
    FtpSession(int client_fd, std::string peer_ip, uint16_t peer_port,
               std::string session_id, std::shared_ptr<SandboxFs> fs,
               std::function<bool(const std::string&, const std::string&, bool&)> auth_validator);
    ~FtpSession();

    // Run the session command loop (blocking on client_fd until disconnect or QUIT)
    void run();

    // Send a reply over the control channel
    bool send_reply(const FtpReply& reply);
    bool send_reply(ReplyCode code, const std::string& message);
    bool send_reply(int code, const std::string& message);

    const std::string& session_id() const { return session_id_; }
    bool is_authenticated() const { return auth_state_ == AuthState::AUTHENTICATED; }

private:
    int client_fd_{-1};
    std::string peer_ip_;
    uint16_t peer_port_{0};
    std::string session_id_;
    std::shared_ptr<SandboxFs> fs_;
    std::function<bool(const std::string&, const std::string&, bool&)> auth_validator_;

    // Session State
    AuthState auth_state_{AuthState::UNAUTHENTICATED};
    std::string pending_user_;
    std::string current_user_;
    bool is_read_only_{false};
    std::string current_vdir_{"/"};
    TransferType transfer_type_{TransferType::BINARY_IMAGE};

    // Data Connection State
    DataMode data_mode_{DataMode::NONE};
    IpPortTuple active_tuple_;
    int pasv_listener_fd_{-1};
    uint16_t pasv_port_{0};

    // Rename state (RNFR -> RNTO)
    std::string rename_from_vpath_;

    // Resume offset (REST)
    uint64_t rest_offset_{0};

    std::atomic<bool> running_{true};

    // Command Dispatcher
    void process_command(const FtpCommand& cmd);

    // Command Handlers
    void handle_user(const FtpCommand& cmd);
    void handle_pass(const FtpCommand& cmd);
    void handle_quit(const FtpCommand& cmd);
    void handle_syst(const FtpCommand& cmd);
    void handle_feat(const FtpCommand& cmd);
    void handle_stat(const FtpCommand& cmd);
    void handle_noop(const FtpCommand& cmd);
    void handle_help(const FtpCommand& cmd);

    void handle_pwd(const FtpCommand& cmd);
    void handle_cwd(const FtpCommand& cmd);
    void handle_cdup(const FtpCommand& cmd);
    void handle_mkd(const FtpCommand& cmd);
    void handle_rmd(const FtpCommand& cmd);

    void handle_type(const FtpCommand& cmd);
    void handle_mode(const FtpCommand& cmd);
    void handle_stru(const FtpCommand& cmd);

    void handle_port(const FtpCommand& cmd);
    void handle_pasv(const FtpCommand& cmd);
    void handle_eprt(const FtpCommand& cmd);
    void handle_epsv(const FtpCommand& cmd);

    void handle_list(const FtpCommand& cmd);
    void handle_nlst(const FtpCommand& cmd);
    void handle_retr(const FtpCommand& cmd);
    void handle_stor(const FtpCommand& cmd);
    void handle_appe(const FtpCommand& cmd);
    void handle_dele(const FtpCommand& cmd);
    void handle_rnfr(const FtpCommand& cmd);
    void handle_rnto(const FtpCommand& cmd);

    void handle_size(const FtpCommand& cmd);
    void handle_mdtm(const FtpCommand& cmd);
    void handle_rest(const FtpCommand& cmd);
    void handle_abor(const FtpCommand& cmd);

    // Data Channel Coordinator
    int establish_data_connection();
    void close_pasv_listener();
};

} // namespace ftp

#endif // FTP_SESSION_HPP
