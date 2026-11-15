#ifndef FTP_SANDBOX_FS_HPP
#define FTP_SANDBOX_FS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

namespace ftp {

struct FileInfo {
    std::string name;
    bool is_directory{false};
    uint64_t size{0};
    time_t mtime{0};
    uint32_t mode{0};
};

class SandboxFs {
public:
    explicit SandboxFs(std::string root_physical_dir);

    const std::string& root_dir() const { return root_physical_dir_; }

    // Normalize a virtual path relative to current working virtual dir
    // Guarantees path stays confined within "/" (cannot escape virtual root)
    std::string normalize_virtual_path(const std::string& vpath, const std::string& current_vdir) const;

    // Convert normalized virtual path to host physical filesystem path
    std::string to_physical_path(const std::string& normalized_vpath) const;

    // Check existence and file type
    bool exists(const std::string& normalized_vpath) const;
    bool is_directory(const std::string& normalized_vpath) const;
    bool is_regular_file(const std::string& normalized_vpath) const;

    // Metadata queries
    bool get_file_info(const std::string& normalized_vpath, FileInfo& info) const;
    uint64_t get_file_size(const std::string& normalized_vpath) const;
    std::string get_mtime_rfc3659(const std::string& normalized_vpath) const; // YYYYMMDDHHMMSS

    // Directory operations
    bool make_directory(const std::string& normalized_vpath);
    bool remove_directory(const std::string& normalized_vpath);
    bool delete_file(const std::string& normalized_vpath);
    bool rename(const std::string& old_vpath, const std::string& new_vpath);

    // Directory listing formatters
    std::string format_unix_list(const std::string& normalized_vdir) const;
    std::vector<std::string> format_name_list(const std::string& normalized_vdir) const;

private:
    std::string root_physical_dir_;

    static std::string format_unix_stat(const FileInfo& file);
};

} // namespace ftp

#endif // FTP_SANDBOX_FS_HPP
