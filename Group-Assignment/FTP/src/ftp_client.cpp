#include "ftp_client.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>

namespace ftp {

FtpClient::FtpClient() {}

FtpClient::~FtpClient() {
    disconnect();
}

bool FtpClient::connect(const std::string& host, uint16_t port) {
    disconnect();
    server_host_ = host;
    server_port_ = port;

    control_fd_ = SocketUtils::connect_tcp(host, port, 10);
    if (control_fd_ < 0) {
        Logger::error("CLIENT", "Failed to connect to " + host + ":" + std::to_string(port));
        return false;
    }

    if (!read_reply(last_reply_) || last_reply_.code() != 220) {
        Logger::error("CLIENT", "Unexpected greeting from FTP server: " + last_reply_.full_message());
        disconnect();
        return false;
    }

    Logger::success("CLIENT", "Connected to FTP server at " + host + ":" + std::to_string(port));
    return true;
}

void FtpClient::disconnect() {
    if (control_fd_ >= 0) {
        send_command("QUIT");
        read_reply(last_reply_);
        SocketUtils::close_socket(control_fd_);
        control_fd_ = -1;
    }
}

bool FtpClient::send_command(const std::string& verb, const std::string& arg) {
    if (control_fd_ < 0) {
        last_reply_ = FtpReply(421, "Not connected to server. Use 'open <host> [port]' to connect.");
        return false;
    }
    FtpCommand cmd(verb, arg);
    std::string wire = cmd.to_wire_string();
    if (!SocketUtils::send_all(control_fd_, wire.data(), wire.size())) {
        SocketUtils::close_socket(control_fd_);
        control_fd_ = -1;
        last_reply_ = FtpReply(421, "Connection to server lost.");
        return false;
    }
    return true;
}

bool FtpClient::read_reply(FtpReply& reply) {
    if (control_fd_ < 0) {
        last_reply_ = FtpReply(421, "Not connected to server.");
        reply = last_reply_;
        return false;
    }
    std::string accumulated;
    std::string line;

    while (SocketUtils::recv_line(control_fd_, line)) {
        accumulated += line + "\r\n";
        size_t consumed = 0;
        if (FtpReply::parse(accumulated, reply, consumed)) {
            last_reply_ = reply;
            return true;
        }
    }

    SocketUtils::close_socket(control_fd_);
    control_fd_ = -1;
    last_reply_ = FtpReply(421, "Connection closed by server or timed out.");
    reply = last_reply_;
    return false;
}

bool FtpClient::login(const std::string& username, const std::string& password) {
    if (!send_command("USER", username)) return false;
    if (!read_reply(last_reply_)) return false;

    if (last_reply_.code() == 230) {
        return true; // Logged in without password
    }

    if (last_reply_.code() == 331) {
        if (!send_command("PASS", password)) return false;
        if (!read_reply(last_reply_)) return false;
        return (last_reply_.code() == 230);
    }

    return false;
}

bool FtpClient::set_type(TransferType type) {
    current_type_ = type;
    std::string t = (type == TransferType::ASCII) ? "A" : "I";
    if (!send_command("TYPE", t)) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 200;
}

int FtpClient::prepare_data_connection() {
    if (passive_mode_) {
        if (!send_command("PASV")) return -1;
        if (!read_reply(last_reply_) || last_reply_.code() != 227) return -1;

        IpPortTuple target;
        if (!IpPortTuple::from_pasv_reply(last_reply_.primary_message(), target)) {
            return -1;
        }

        // When server replies with 127.0.0.1 or local network, connect to that target
        return SocketUtils::connect_tcp(target.ip, target.port, 10);
    } else {
        // Active mode: create local listener socket
        std::string local_ip;
        uint16_t dummy;
        if (!SocketUtils::get_socket_address(control_fd_, local_ip, dummy)) {
            local_ip = "127.0.0.1";
        }

        int listener_fd = SocketUtils::create_tcp_listener(local_ip, 0, 1);
        if (listener_fd < 0) return -1;

        uint16_t listen_port = 0;
        if (!SocketUtils::get_socket_address(listener_fd, local_ip, listen_port)) {
            SocketUtils::close_socket(listener_fd);
            return -1;
        }

        IpPortTuple tuple(local_ip, listen_port);
        if (!send_command("PORT", tuple.to_comma_format())) {
            SocketUtils::close_socket(listener_fd);
            return -1;
        }
        if (!read_reply(last_reply_) || last_reply_.code() != 200) {
            SocketUtils::close_socket(listener_fd);
            return -1;
        }

        return listener_fd; // In active mode, caller accepts connection
    }
}

bool FtpClient::pwd(std::string& current_dir) {
    if (!send_command("PWD")) return false;
    if (!read_reply(last_reply_) || last_reply_.code() != 257) return false;

    // Parse directory inside quotes
    std::string msg = last_reply_.primary_message();
    size_t q1 = msg.find('"');
    size_t q2 = msg.rfind('"');
    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
        current_dir = msg.substr(q1 + 1, q2 - q1 - 1);
    } else {
        current_dir = msg;
    }
    return true;
}

bool FtpClient::cwd(const std::string& path) {
    if (!send_command("CWD", path)) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 250;
}

bool FtpClient::cdup() {
    if (!send_command("CDUP")) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 250;
}

bool FtpClient::mkdir(const std::string& dir_path) {
    if (!send_command("MKD", dir_path)) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 257;
}

bool FtpClient::rmdir(const std::string& dir_path) {
    if (!send_command("RMD", dir_path)) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 250;
}

bool FtpClient::list(const std::string& path, std::string& listing_out) {
    listing_out.clear();
    int data_fd = prepare_data_connection();
    if (data_fd < 0) return false;

    if (!send_command("LIST", path)) {
        SocketUtils::close_socket(data_fd);
        return false;
    }
    if (!read_reply(last_reply_) || last_reply_.code() != 150) {
        SocketUtils::close_socket(data_fd);
        return false;
    }

    int active_conn_fd = -1;
    if (!passive_mode_) {
        // Active mode: accept data connection from server
        std::string s_ip;
        uint16_t s_port;
        active_conn_fd = SocketUtils::accept_client(data_fd, s_ip, s_port);
        SocketUtils::close_socket(data_fd);
        data_fd = active_conn_fd;
    }

    char buf[4096];
    ssize_t n;
    while ((n = SocketUtils::recv_chunk(data_fd, buf, sizeof(buf))) > 0) {
        listing_out.append(buf, n);
    }
    SocketUtils::close_socket(data_fd);

    if (!read_reply(last_reply_) || last_reply_.code() != 226) {
        return false;
    }
    return true;
}

bool FtpClient::nlist(const std::string& path, std::vector<std::string>& files_out) {
    files_out.clear();
    std::string raw;
    if (!list(path, raw)) return false;

    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        line = trim(line);
        if (!line.empty()) {
            files_out.push_back(line);
        }
    }
    return true;
}

bool FtpClient::download_file(const std::string& remote_file, const std::string& local_file,
                             uint64_t resume_offset,
                             std::function<void(uint64_t, uint64_t)> progress_cb) {
    if (resume_offset > 0) {
        if (!send_command("REST", std::to_string(resume_offset))) return false;
        if (!read_reply(last_reply_) || last_reply_.code() != 350) return false;
    }

    int data_fd = prepare_data_connection();
    if (data_fd < 0) return false;

    if (!send_command("RETR", remote_file)) {
        SocketUtils::close_socket(data_fd);
        return false;
    }
    if (!read_reply(last_reply_) || last_reply_.code() != 150) {
        SocketUtils::close_socket(data_fd);
        return false;
    }

    if (!passive_mode_) {
        std::string s_ip;
        uint16_t s_port;
        int active_conn = SocketUtils::accept_client(data_fd, s_ip, s_port);
        SocketUtils::close_socket(data_fd);
        data_fd = active_conn;
    }

    std::ofstream ofs(local_file, std::ios::binary | (resume_offset > 0 ? std::ios::app : std::ios::trunc));
    if (!ofs.is_open()) {
        SocketUtils::close_socket(data_fd);
        return false;
    }

    char buf[BUFFER_SIZE];
    ssize_t n;
    uint64_t total_bytes = resume_offset;

    while ((n = SocketUtils::recv_chunk(data_fd, buf, sizeof(buf))) > 0) {
        ofs.write(buf, n);
        total_bytes += n;
        if (progress_cb) progress_cb(total_bytes, 0);
    }

    ofs.close();
    SocketUtils::close_socket(data_fd);

    if (!read_reply(last_reply_) || last_reply_.code() != 226) {
        return false;
    }
    return true;
}

bool FtpClient::upload_file(const std::string& local_file, const std::string& remote_file,
                           bool append,
                           std::function<void(uint64_t, uint64_t)> progress_cb) {
    std::ifstream ifs(local_file, std::ios::binary);
    if (!ifs.is_open()) {
        last_reply_ = FtpReply(550, "Local file '" + local_file + "' not found or cannot be opened.");
        return false;
    }

    ifs.seekg(0, std::ios::end);
    uint64_t file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    int data_fd = prepare_data_connection();
    if (data_fd < 0) return false;

    std::string cmd_verb = append ? "APPE" : "STOR";
    if (!send_command(cmd_verb, remote_file)) {
        SocketUtils::close_socket(data_fd);
        return false;
    }
    if (!read_reply(last_reply_) || last_reply_.code() != 150) {
        SocketUtils::close_socket(data_fd);
        return false;
    }

    if (!passive_mode_) {
        std::string s_ip;
        uint16_t s_port;
        int active_conn = SocketUtils::accept_client(data_fd, s_ip, s_port);
        SocketUtils::close_socket(data_fd);
        data_fd = active_conn;
    }

    char buf[BUFFER_SIZE];
    uint64_t total_sent = 0;

    while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
        size_t count = ifs.gcount();
        if (!SocketUtils::send_all(data_fd, buf, count)) {
            SocketUtils::close_socket(data_fd);
            return false;
        }
        total_sent += count;
        if (progress_cb) progress_cb(total_sent, file_size);
    }

    SocketUtils::close_socket(data_fd);
    if (!read_reply(last_reply_) || last_reply_.code() != 226) {
        return false;
    }
    return true;
}

bool FtpClient::delete_file(const std::string& remote_file) {
    if (!send_command("DELE", remote_file)) return false;
    if (!read_reply(last_reply_)) return false;
    return last_reply_.code() == 250;
}

bool FtpClient::rename_file(const std::string& old_name, const std::string& new_name) {
    if (!send_command("RNFR", old_name)) return false;
    if (!read_reply(last_reply_) || last_reply_.code() != 350) return false;

    if (!send_command("RNTO", new_name)) return false;
    if (!read_reply(last_reply_) || last_reply_.code() != 250) return false;
    return true;
}

bool FtpClient::get_size(const std::string& remote_file, uint64_t& size_out) {
    if (!send_command("SIZE", remote_file)) return false;
    if (!read_reply(last_reply_) || last_reply_.code() != 213) return false;
    try {
        size_out = std::stoull(last_reply_.primary_message());
        return true;
    } catch (...) {
        return false;
    }
}

bool FtpClient::get_mod_time(const std::string& remote_file, std::string& mtime_out) {
    if (!send_command("MDTM", remote_file)) return false;
    if (!read_reply(last_reply_) || last_reply_.code() != 213) return false;
    mtime_out = last_reply_.primary_message();
    return true;
}

} // namespace ftp
