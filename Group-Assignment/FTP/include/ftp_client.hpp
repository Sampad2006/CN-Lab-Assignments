#ifndef FTP_CLIENT_HPP
#define FTP_CLIENT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "common.hpp"
#include "ftp_command.hpp"
#include "ftp_reply.hpp"
#include "socket_utils.hpp"

namespace ftp {

class FtpClient {
public:
    FtpClient();
    ~FtpClient();

    // Session establishment
    bool connect(const std::string& host, uint16_t port = DEFAULT_FTP_PORT);
    bool login(const std::string& username, const std::string& password = "");
    bool is_connected() const { return control_fd_ >= 0; }
    void disconnect();

    // Transfer configuration
    void set_passive(bool passive) { passive_mode_ = passive; }
    bool is_passive() const { return passive_mode_; }
    bool set_type(TransferType type);

    // Directory operations
    bool pwd(std::string& current_dir);
    bool cwd(const std::string& path);
    bool cdup();
    bool mkdir(const std::string& dir_path);
    bool rmdir(const std::string& dir_path);

    // Listing
    bool list(const std::string& path, std::string& listing_out);
    bool nlist(const std::string& path, std::vector<std::string>& files_out);

    // File transfer
    bool download_file(const std::string& remote_file, const std::string& local_file,
                       uint64_t resume_offset = 0,
                       std::function<void(uint64_t, uint64_t)> progress_cb = nullptr);

    bool upload_file(const std::string& local_file, const std::string& remote_file,
                     bool append = false,
                     std::function<void(uint64_t, uint64_t)> progress_cb = nullptr);

    // File manipulation & metadata
    bool delete_file(const std::string& remote_file);
    bool rename_file(const std::string& old_name, const std::string& new_name);
    bool get_size(const std::string& remote_file, uint64_t& size_out);
    bool get_mod_time(const std::string& remote_file, std::string& mtime_out);

    // Raw protocol interaction
    bool send_command(const std::string& verb, const std::string& arg = "");
    bool read_reply(FtpReply& reply);
    const FtpReply& last_reply() const { return last_reply_; }

private:
    int control_fd_{-1};
    std::string server_host_;
    uint16_t server_port_{0};
    bool passive_mode_{true};
    TransferType current_type_{TransferType::BINARY_IMAGE};
    FtpReply last_reply_;

    // Data connection setup
    int prepare_data_connection();
};

} // namespace ftp

#endif // FTP_CLIENT_HPP
