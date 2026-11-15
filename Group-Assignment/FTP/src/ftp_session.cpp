#include "ftp_session.hpp"
#include "logger.hpp"
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <vector>

namespace ftp {

FtpSession::FtpSession(int client_fd, std::string peer_ip, uint16_t peer_port,
                       std::string session_id, std::shared_ptr<SandboxFs> fs,
                       std::function<bool(const std::string&, const std::string&, bool&)> auth_validator)
    : client_fd_(client_fd),
      peer_ip_(std::move(peer_ip)),
      peer_port_(peer_port),
      session_id_(std::move(session_id)),
      fs_(std::move(fs)),
      auth_validator_(std::move(auth_validator)) {
}

FtpSession::~FtpSession() {
    close_pasv_listener();
    if (client_fd_ >= 0) {
        SocketUtils::close_socket(client_fd_);
    }
}

void FtpSession::close_pasv_listener() {
    if (pasv_listener_fd_ >= 0) {
        SocketUtils::close_socket(pasv_listener_fd_);
        pasv_port_ = 0;
    }
}

bool FtpSession::send_reply(const FtpReply& reply) {
    if (client_fd_ < 0) return false;
    std::string wire = reply.to_wire_string();
    Logger::reply(session_id_, reply.code(), reply.primary_message());
    return SocketUtils::send_all(client_fd_, wire.data(), wire.size());
}

bool FtpSession::send_reply(ReplyCode code, const std::string& message) {
    return send_reply(FtpReply(code, message));
}

bool FtpSession::send_reply(int code, const std::string& message) {
    return send_reply(FtpReply(code, message));
}

void FtpSession::run() {
    Logger::info("SESSION", "Client connected from " + peer_ip_ + ":" + std::to_string(peer_port_) +
                            " [Session " + session_id_ + "]");

    // Initial RFC 959 greeting banner
    send_reply(ReplyCode::SERVICE_READY, "JU Computer Networks Laboratory FTP Server (RFC 959)");

    std::string line;
    while (running_ && SocketUtils::recv_line(client_fd_, line)) {
        if (line.empty()) continue;

        FtpCommand cmd;
        if (!FtpCommand::parse(line, cmd)) {
            send_reply(ReplyCode::SYNTAX_ERROR_COMMAND, "Unrecognized command line.");
            continue;
        }

        Logger::command(session_id_, cmd.verb() + (cmd.argument().empty() ? "" : " " + cmd.argument()));
        process_command(cmd);
    }

    Logger::info("SESSION", "Session closed for " + session_id_);
}

void FtpSession::process_command(const FtpCommand& cmd) {
    // Commands permitted before authentication:
    if (cmd.is("USER")) { handle_user(cmd); return; }
    if (cmd.is("PASS")) { handle_pass(cmd); return; }
    if (cmd.is("QUIT")) { handle_quit(cmd); return; }
    if (cmd.is("NOOP")) { handle_noop(cmd); return; }
    if (cmd.is("HELP")) { handle_help(cmd); return; }
    if (cmd.is("SYST")) { handle_syst(cmd); return; }
    if (cmd.is("FEAT")) { handle_feat(cmd); return; }

    // Enforce authentication for all other commands
    if (auth_state_ != AuthState::AUTHENTICATED) {
        send_reply(ReplyCode::NOT_LOGGED_IN, "Please login with USER and PASS first.");
        return;
    }

    // Authenticated commands
    if (cmd.is("PWD") || cmd.is("XPWD")) { handle_pwd(cmd); return; }
    if (cmd.is("CWD"))  { handle_cwd(cmd); return; }
    if (cmd.is("CDUP") || cmd.is("XCUP")) { handle_cdup(cmd); return; }
    if (cmd.is("MKD") || cmd.is("XMKD"))  { handle_mkd(cmd); return; }
    if (cmd.is("RMD") || cmd.is("XRMD"))  { handle_rmd(cmd); return; }

    if (cmd.is("TYPE")) { handle_type(cmd); return; }
    if (cmd.is("MODE")) { handle_mode(cmd); return; }
    if (cmd.is("STRU")) { handle_stru(cmd); return; }

    if (cmd.is("PASV")) { handle_pasv(cmd); return; }
    if (cmd.is("PORT")) { handle_port(cmd); return; }
    if (cmd.is("EPSV")) { handle_epsv(cmd); return; }
    if (cmd.is("EPRT")) { handle_eprt(cmd); return; }

    if (cmd.is("LIST")) { handle_list(cmd); return; }
    if (cmd.is("NLST")) { handle_nlst(cmd); return; }
    if (cmd.is("RETR")) { handle_retr(cmd); return; }
    if (cmd.is("STOR")) { handle_stor(cmd); return; }
    if (cmd.is("APPE")) { handle_appe(cmd); return; }
    if (cmd.is("DELE")) { handle_dele(cmd); return; }
    if (cmd.is("RNFR")) { handle_rnfr(cmd); return; }
    if (cmd.is("RNTO")) { handle_rnto(cmd); return; }

    if (cmd.is("SIZE")) { handle_size(cmd); return; }
    if (cmd.is("MDTM")) { handle_mdtm(cmd); return; }
    if (cmd.is("REST")) { handle_rest(cmd); return; }
    if (cmd.is("ABOR")) { handle_abor(cmd); return; }
    if (cmd.is("STAT")) { handle_stat(cmd); return; }

    send_reply(ReplyCode::COMMAND_NOT_IMPLEMENTED, "Command '" + cmd.verb() + "' not implemented.");
}

void FtpSession::handle_user(const FtpCommand& cmd) {
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "USER requires a username.");
        return;
    }
    pending_user_ = cmd.argument();
    auth_state_ = AuthState::AWAITING_PASSWORD;
    send_reply(ReplyCode::USER_NAME_OK_NEED_PASS, "User name okay, need password.");
}

void FtpSession::handle_pass(const FtpCommand& cmd) {
    if (auth_state_ != AuthState::AWAITING_PASSWORD) {
        send_reply(ReplyCode::BAD_SEQUENCE_OF_COMMANDS, "Login with USER first.");
        return;
    }

    bool ro = false;
    if (auth_validator_ && auth_validator_(pending_user_, cmd.argument(), ro)) {
        current_user_ = pending_user_;
        is_read_only_ = ro;
        auth_state_ = AuthState::AUTHENTICATED;
        send_reply(ReplyCode::USER_LOGGED_IN, "User " + current_user_ + " logged in successfully." +
                                              (is_read_only_ ? " [Read-Only]" : " [Read-Write]"));
    } else {
        auth_state_ = AuthState::UNAUTHENTICATED;
        pending_user_.clear();
        send_reply(ReplyCode::NOT_LOGGED_IN, "Authentication failed.");
    }
}

void FtpSession::handle_quit(const FtpCommand&) {
    send_reply(ReplyCode::CLOSING_CONTROL_CONN, "JU FTP Server closing connection. Goodbye.");
    running_ = false;
}

void FtpSession::handle_syst(const FtpCommand&) {
    send_reply(ReplyCode::SYSTEM_NAME_TYPE, "UNIX Type: L8");
}

void FtpSession::handle_feat(const FtpCommand&) {
    std::vector<std::string> feat_lines = {
        "Features:",
        " EPRT",
        " EPSV",
        " MDTM",
        " PASV",
        " REST STREAM",
        " SIZE",
        " TVFS",
        " UTF8",
        "End"
    };
    send_reply(FtpReply(211, feat_lines));
}

void FtpSession::handle_stat(const FtpCommand&) {
    std::vector<std::string> stat_lines = {
        "JU FTP Server Status:",
        " Connected from " + peer_ip_ + ":" + std::to_string(peer_port_),
        " Logged in as: " + current_user_,
        " TYPE: " + std::string(transfer_type_ == TransferType::BINARY_IMAGE ? "Binary (IMAGE)" : "ASCII"),
        " Current Directory: " + current_vdir_,
        "End of status"
    };
    send_reply(FtpReply(211, stat_lines));
}

void FtpSession::handle_noop(const FtpCommand&) {
    send_reply(ReplyCode::COMMAND_OK, "NOOP ok.");
}

void FtpSession::handle_help(const FtpCommand&) {
    std::vector<std::string> help_lines = {
        "Supported RFC 959 commands:",
        " USER, PASS, QUIT, PWD, CWD, CDUP, MKD, RMD,",
        " TYPE, MODE, STRU, PORT, PASV, EPRT, EPSV,",
        " LIST, NLST, RETR, STOR, APPE, DELE, RNFR, RNTO,",
        " SIZE, MDTM, REST, ABOR, SYST, FEAT, STAT, NOOP",
        "Direct queries to CSE Jadavpur University"
    };
    send_reply(FtpReply(214, help_lines));
}

void FtpSession::handle_pwd(const FtpCommand&) {
    send_reply(ReplyCode::PATHNAME_CREATED, "\"" + current_vdir_ + "\" is current directory.");
}

void FtpSession::handle_cwd(const FtpCommand& cmd) {
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "CWD requires path argument.");
        return;
    }
    std::string target_vdir = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target_vdir) || !fs_->is_directory(target_vdir)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Directory not found or not accessible.");
        return;
    }
    current_vdir_ = target_vdir;
    send_reply(ReplyCode::REQUESTED_FILE_ACTION_OK, "CWD command successful. Current directory is \"" + current_vdir_ + "\".");
}

void FtpSession::handle_cdup(const FtpCommand&) {
    current_vdir_ = fs_->normalize_virtual_path("..", current_vdir_);
    send_reply(ReplyCode::REQUESTED_FILE_ACTION_OK, "CDUP command successful. Current directory is \"" + current_vdir_ + "\".");
}

void FtpSession::handle_mkd(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only account).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "MKD requires directory name.");
        return;
    }
    std::string target = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (fs_->exists(target)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Directory or file already exists.");
        return;
    }
    if (fs_->make_directory(target)) {
        send_reply(ReplyCode::PATHNAME_CREATED, "\"" + target + "\" directory created.");
    } else {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Failed to create directory.");
    }
}

void FtpSession::handle_rmd(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only account).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "RMD requires directory name.");
        return;
    }
    std::string target = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target) || !fs_->is_directory(target)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Target directory does not exist.");
        return;
    }
    if (target == "/") {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Cannot remove root directory.");
        return;
    }
    if (fs_->remove_directory(target)) {
        send_reply(ReplyCode::REQUESTED_FILE_ACTION_OK, "Directory removed.");
    } else {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Failed to remove directory (may not be empty).");
    }
}

void FtpSession::handle_type(const FtpCommand& cmd) {
    std::string arg = to_upper(trim(cmd.argument()));
    if (arg == "A" || arg == "A N") {
        transfer_type_ = TransferType::ASCII;
        send_reply(ReplyCode::COMMAND_OK, "Switching to ASCII mode.");
    } else if (arg == "I" || arg == "L 8" || arg == "L 8") {
        transfer_type_ = TransferType::BINARY_IMAGE;
        send_reply(ReplyCode::COMMAND_OK, "Switching to Binary (IMAGE) mode.");
    } else {
        send_reply(ReplyCode::COMMAND_NOT_IMPLEMENTED_PAR, "Type not supported. Use A (ASCII) or I (Binary).");
    }
}

void FtpSession::handle_mode(const FtpCommand& cmd) {
    if (to_upper(trim(cmd.argument())) == "S") {
        send_reply(ReplyCode::COMMAND_OK, "Mode set to S (Stream).");
    } else {
        send_reply(ReplyCode::COMMAND_NOT_IMPLEMENTED_PAR, "Only Stream (S) mode is supported.");
    }
}

void FtpSession::handle_stru(const FtpCommand& cmd) {
    if (to_upper(trim(cmd.argument())) == "F") {
        send_reply(ReplyCode::COMMAND_OK, "Structure set to F (File).");
    } else {
        send_reply(ReplyCode::COMMAND_NOT_IMPLEMENTED_PAR, "Only File (F) structure is supported.");
    }
}

void FtpSession::handle_pasv(const FtpCommand&) {
    close_pasv_listener();

    // Bind listener on local server IP, ephemeral port (0)
    std::string server_ip;
    uint16_t dummy_port;
    if (!SocketUtils::get_socket_address(client_fd_, server_ip, dummy_port)) {
        server_ip = "127.0.0.1";
    }

    pasv_listener_fd_ = SocketUtils::create_tcp_listener(server_ip, 0, 1);
    if (pasv_listener_fd_ < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Unable to open passive listening port.");
        return;
    }

    if (!SocketUtils::get_socket_address(pasv_listener_fd_, server_ip, pasv_port_)) {
        close_pasv_listener();
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Unable to determine passive port.");
        return;
    }

    data_mode_ = DataMode::PASSIVE;
    IpPortTuple tuple(server_ip, pasv_port_);
    send_reply(ReplyCode::ENTERING_PASSIVE_MODE, "Entering Passive Mode (" + tuple.to_comma_format() + ").");
}

void FtpSession::handle_epsv(const FtpCommand&) {
    close_pasv_listener();

    std::string server_ip;
    uint16_t dummy_port;
    if (!SocketUtils::get_socket_address(client_fd_, server_ip, dummy_port)) {
        server_ip = "127.0.0.1";
    }

    pasv_listener_fd_ = SocketUtils::create_tcp_listener(server_ip, 0, 1);
    if (pasv_listener_fd_ < 0 || !SocketUtils::get_socket_address(pasv_listener_fd_, server_ip, pasv_port_)) {
        close_pasv_listener();
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Unable to initialize extended passive mode.");
        return;
    }

    data_mode_ = DataMode::EXTENDED_PASSIVE;
    send_reply(ReplyCode::ENTERING_EXTENDED_PASSIVE, "Entering Extended Passive Mode (|||" + std::to_string(pasv_port_) + "|).");
}

void FtpSession::handle_port(const FtpCommand& cmd) {
    close_pasv_listener();
    if (!IpPortTuple::from_comma_format(cmd.argument(), active_tuple_)) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "Illegal PORT command format. Use h1,h2,h3,h4,p1,p2.");
        return;
    }

    data_mode_ = DataMode::ACTIVE;
    send_reply(ReplyCode::COMMAND_OK, "PORT command successful (" + active_tuple_.to_string() + ").");
}

void FtpSession::handle_eprt(const FtpCommand& cmd) {
    close_pasv_listener();
    // RFC 2428: EPRT |<net-prt>|<net-addr>|<tcp-port>|
    std::string arg = trim(cmd.argument());
    if (arg.size() < 7 || arg.front() != '|' || arg.back() != '|') {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "Illegal EPRT format.");
        return;
    }
    char delim = arg.front();
    auto parts = split(arg, delim);
    // parts[0]="", parts[1]="1", parts[2]=ip, parts[3]=port, parts[4]=""
    if (parts.size() < 4) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "Illegal EPRT parameters.");
        return;
    }

    active_tuple_.ip = parts[2];
    active_tuple_.port = static_cast<uint16_t>(std::stoi(parts[3]));
    data_mode_ = DataMode::ACTIVE;
    send_reply(ReplyCode::COMMAND_OK, "EPRT command successful.");
}

int FtpSession::establish_data_connection() {
    int data_fd = -1;
    if (data_mode_ == DataMode::PASSIVE || data_mode_ == DataMode::EXTENDED_PASSIVE) {
        if (pasv_listener_fd_ < 0) return -1;
        std::string client_data_ip;
        uint16_t client_data_port;
        data_fd = SocketUtils::accept_client(pasv_listener_fd_, client_data_ip, client_data_port);
        close_pasv_listener();
    } else if (data_mode_ == DataMode::ACTIVE) {
        data_fd = SocketUtils::connect_tcp(active_tuple_.ip, active_tuple_.port, 10);
    }
    data_mode_ = DataMode::NONE;
    return data_fd;
}

void FtpSession::handle_list(const FtpCommand& cmd) {
    std::string target_vdir = current_vdir_;
    if (!cmd.argument().empty()) {
        std::string arg = cmd.argument();
        if (arg.front() != '-') { // Ignore flags like -l or -a
            target_vdir = fs_->normalize_virtual_path(arg, current_vdir_);
        }
    }

    if (!fs_->exists(target_vdir)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Directory not found.");
        return;
    }

    std::string listing = fs_->format_unix_list(target_vdir);

    send_reply(ReplyCode::FILE_STATUS_OK_OPEN_DATA, "Opening data connection for directory list.");

    int data_fd = establish_data_connection();
    if (data_fd < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Can't open data connection.");
        return;
    }

    if (!listing.empty()) {
        SocketUtils::send_all(data_fd, listing.data(), listing.size());
    }
    SocketUtils::close_socket(data_fd);
    Logger::data(session_id_, "TX", listing.size(), "Directory LIST");

    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "Transfer complete.");
}

void FtpSession::handle_nlst(const FtpCommand& cmd) {
    std::string target_vdir = current_vdir_;
    if (!cmd.argument().empty()) {
        target_vdir = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    }

    if (!fs_->exists(target_vdir)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Directory not found.");
        return;
    }

    auto names = fs_->format_name_list(target_vdir);
    std::string nlisting;
    for (const auto& n : names) {
        nlisting += n + "\r\n";
    }

    send_reply(ReplyCode::FILE_STATUS_OK_OPEN_DATA, "Opening data connection for file names list.");

    int data_fd = establish_data_connection();
    if (data_fd < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Can't open data connection.");
        return;
    }

    if (!nlisting.empty()) {
        SocketUtils::send_all(data_fd, nlisting.data(), nlisting.size());
    }
    SocketUtils::close_socket(data_fd);
    Logger::data(session_id_, "TX", nlisting.size(), "NLST");

    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "Transfer complete.");
}

void FtpSession::handle_retr(const FtpCommand& cmd) {
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "RETR requires filename.");
        return;
    }

    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target_vpath) || !fs_->is_regular_file(target_vpath)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "File not found or access denied.");
        return;
    }

    std::string ppath = fs_->to_physical_path(target_vpath);
    std::ifstream ifs(ppath, std::ios::binary);
    if (!ifs.is_open()) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Could not open local file.");
        return;
    }

    uint64_t file_size = fs_->get_file_size(target_vpath);
    if (rest_offset_ > 0) {
        if (rest_offset_ >= file_size) {
            send_reply(ReplyCode::FILE_ACTION_NOT_TAKEN_TEMP, "REST marker exceeds file length.");
            rest_offset_ = 0;
            return;
        }
        ifs.seekg(rest_offset_);
    }
    uint64_t initial_offset = rest_offset_;
    rest_offset_ = 0; // Consume marker

    send_reply(ReplyCode::FILE_STATUS_OK_OPEN_DATA,
               "Opening BINARY mode data connection for " + target_vpath +
               " (" + std::to_string(file_size - initial_offset) + " bytes).");

    int data_fd = establish_data_connection();
    if (data_fd < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Can't open data connection.");
        return;
    }

    char buffer[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount() > 0) {
        size_t count = ifs.gcount();
        if (!SocketUtils::send_all(data_fd, buffer, count)) {
            SocketUtils::close_socket(data_fd);
            send_reply(ReplyCode::CONN_CLOSED_TRANSFER_ABORT, "Transfer aborted prematurely.");
            return;
        }
        total_sent += count;
    }

    SocketUtils::close_socket(data_fd);
    Logger::data(session_id_, "TX", total_sent, "RETR " + target_vpath);
    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "Transfer complete (" + std::to_string(total_sent) + " bytes sent).");
}

void FtpSession::handle_stor(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only user).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "STOR requires filename.");
        return;
    }

    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    std::string ppath = fs_->to_physical_path(target_vpath);

    std::ofstream ofs(ppath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Cannot open remote file for writing.");
        return;
    }

    send_reply(ReplyCode::FILE_STATUS_OK_OPEN_DATA, "Opening BINARY mode data connection for upload.");

    int data_fd = establish_data_connection();
    if (data_fd < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Can't open data connection.");
        return;
    }

    char buffer[BUFFER_SIZE];
    uint64_t total_received = 0;
    ssize_t bytes_read = 0;

    while ((bytes_read = SocketUtils::recv_chunk(data_fd, buffer, sizeof(buffer))) > 0) {
        ofs.write(buffer, bytes_read);
        total_received += bytes_read;
    }

    ofs.close();
    SocketUtils::close_socket(data_fd);
    Logger::data(session_id_, "RX", total_received, "STOR " + target_vpath);

    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "Transfer complete (" + std::to_string(total_received) + " bytes received).");
}

void FtpSession::handle_appe(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only user).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "APPE requires filename.");
        return;
    }

    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    std::string ppath = fs_->to_physical_path(target_vpath);

    std::ofstream ofs(ppath, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Cannot open remote file for appending.");
        return;
    }

    send_reply(ReplyCode::FILE_STATUS_OK_OPEN_DATA, "Opening data connection for APPEND.");

    int data_fd = establish_data_connection();
    if (data_fd < 0) {
        send_reply(ReplyCode::CANNOT_OPEN_DATA_CONN, "Can't open data connection.");
        return;
    }

    char buffer[BUFFER_SIZE];
    uint64_t total_received = 0;
    ssize_t bytes_read = 0;

    while ((bytes_read = SocketUtils::recv_chunk(data_fd, buffer, sizeof(buffer))) > 0) {
        ofs.write(buffer, bytes_read);
        total_received += bytes_read;
    }

    ofs.close();
    SocketUtils::close_socket(data_fd);
    Logger::data(session_id_, "RX", total_received, "APPE " + target_vpath);

    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "Append complete (" + std::to_string(total_received) + " bytes appended).");
}

void FtpSession::handle_dele(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only user).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "DELE requires filename.");
        return;
    }

    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target_vpath) || fs_->is_directory(target_vpath)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "File does not exist or is a directory.");
        return;
    }

    if (fs_->delete_file(target_vpath)) {
        send_reply(ReplyCode::REQUESTED_FILE_ACTION_OK, "File deleted successfully.");
    } else {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Delete failed.");
    }
}

void FtpSession::handle_rnfr(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only user).");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "RNFR requires source path.");
        return;
    }

    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target_vpath)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "File or folder does not exist.");
        return;
    }

    rename_from_vpath_ = target_vpath;
    send_reply(ReplyCode::FILE_ACTION_PENDING_INFO, "File exists, ready for destination name (RNTO).");
}

void FtpSession::handle_rnto(const FtpCommand& cmd) {
    if (is_read_only_) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Permission denied (Read-Only user).");
        return;
    }
    if (rename_from_vpath_.empty()) {
        send_reply(ReplyCode::BAD_SEQUENCE_OF_COMMANDS, "RNFR required prior to RNTO.");
        return;
    }
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "RNTO requires destination path.");
        return;
    }

    std::string target_to = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (fs_->rename(rename_from_vpath_, target_to)) {
        send_reply(ReplyCode::REQUESTED_FILE_ACTION_OK, "Renamed " + rename_from_vpath_ + " to " + target_to + ".");
        rename_from_vpath_.clear();
    } else {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "Rename operation failed.");
    }
}

void FtpSession::handle_size(const FtpCommand& cmd) {
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "SIZE requires filename.");
        return;
    }
    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    if (!fs_->exists(target_vpath) || fs_->is_directory(target_vpath)) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "File unavailable or is directory.");
        return;
    }
    uint64_t size = fs_->get_file_size(target_vpath);
    send_reply(ReplyCode::FILE_STATUS, std::to_string(size));
}

void FtpSession::handle_mdtm(const FtpCommand& cmd) {
    if (cmd.argument().empty()) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "MDTM requires filename.");
        return;
    }
    std::string target_vpath = fs_->normalize_virtual_path(cmd.argument(), current_vdir_);
    std::string mtime = fs_->get_mtime_rfc3659(target_vpath);
    if (mtime.empty()) {
        send_reply(ReplyCode::FILE_UNAVAILABLE, "File unavailable.");
    } else {
        send_reply(ReplyCode::FILE_STATUS, mtime);
    }
}

void FtpSession::handle_rest(const FtpCommand& cmd) {
    try {
        uint64_t offset = std::stoull(cmd.argument());
        rest_offset_ = offset;
        send_reply(ReplyCode::FILE_ACTION_PENDING_INFO, "Restarting at " + std::to_string(offset) + ". Send STORE or RETRIEVE.");
    } catch (...) {
        send_reply(ReplyCode::SYNTAX_ERROR_PARAMETERS, "Invalid REST parameter offset.");
    }
}

void FtpSession::handle_abor(const FtpCommand&) {
    close_pasv_listener();
    send_reply(ReplyCode::CLOSING_DATA_CONN_SUCCESS, "ABOR command acknowledged.");
}

} // namespace ftp
