#include "sandbox_fs.hpp"
#include "common.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>

namespace ftp {

SandboxFs::SandboxFs(std::string root_physical_dir)
    : root_physical_dir_(std::move(root_physical_dir)) {
    // Strip trailing slashes from root_physical_dir
    while (root_physical_dir_.size() > 1 && root_physical_dir_.back() == '/') {
        root_physical_dir_.pop_back();
    }
    // Ensure root directory exists
    struct stat st;
    if (::stat(root_physical_dir_.c_str(), &st) != 0) {
        ::mkdir(root_physical_dir_.c_str(), 0755);
    }
}

std::string SandboxFs::normalize_virtual_path(const std::string& vpath, const std::string& current_vdir) const {
    std::string base;
    if (!vpath.empty() && vpath.front() == '/') {
        base = vpath;
    } else {
        base = current_vdir + "/" + vpath;
    }

    std::vector<std::string> parts = split(base, '/');
    std::vector<std::string> stack;

    for (const auto& part : parts) {
        if (part.empty() || part == ".") {
            continue;
        }
        if (part == "..") {
            if (!stack.empty()) {
                stack.pop_back();
            }
        } else {
            stack.push_back(part);
        }
    }

    if (stack.empty()) {
        return "/";
    }

    std::string res;
    for (const auto& item : stack) {
        res += "/" + item;
    }
    return res;
}

std::string SandboxFs::to_physical_path(const std::string& normalized_vpath) const {
    if (normalized_vpath == "/" || normalized_vpath.empty()) {
        return root_physical_dir_;
    }
    return root_physical_dir_ + normalized_vpath;
}

bool SandboxFs::exists(const std::string& normalized_vpath) const {
    std::string ppath = to_physical_path(normalized_vpath);
    struct stat st;
    return (::stat(ppath.c_str(), &st) == 0);
}

bool SandboxFs::is_directory(const std::string& normalized_vpath) const {
    std::string ppath = to_physical_path(normalized_vpath);
    struct stat st;
    if (::stat(ppath.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool SandboxFs::is_regular_file(const std::string& normalized_vpath) const {
    std::string ppath = to_physical_path(normalized_vpath);
    struct stat st;
    if (::stat(ppath.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

bool SandboxFs::get_file_info(const std::string& normalized_vpath, FileInfo& info) const {
    std::string ppath = to_physical_path(normalized_vpath);
    struct stat st;
    if (::stat(ppath.c_str(), &st) != 0) return false;

    // Extract filename from normalized virtual path
    size_t last_slash = normalized_vpath.find_last_of('/');
    if (last_slash != std::string::npos && last_slash + 1 < normalized_vpath.size()) {
        info.name = normalized_vpath.substr(last_slash + 1);
    } else {
        info.name = normalized_vpath;
    }

    info.is_directory = S_ISDIR(st.st_mode);
    info.size = static_cast<uint64_t>(st.st_size);
    info.mtime = st.st_mtime;
    info.mode = st.st_mode;
    return true;
}

uint64_t SandboxFs::get_file_size(const std::string& normalized_vpath) const {
    FileInfo info;
    if (get_file_info(normalized_vpath, info)) {
        return info.size;
    }
    return 0;
}

std::string SandboxFs::get_mtime_rfc3659(const std::string& normalized_vpath) const {
    FileInfo info;
    if (!get_file_info(normalized_vpath, info)) return "";

    struct tm tm_buf;
    gmtime_r(&info.mtime, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm_buf);
    return std::string(buf);
}

bool SandboxFs::make_directory(const std::string& normalized_vpath) {
    std::string ppath = to_physical_path(normalized_vpath);
    return (::mkdir(ppath.c_str(), 0755) == 0);
}

bool SandboxFs::remove_directory(const std::string& normalized_vpath) {
    if (normalized_vpath == "/") return false; // Prevent removing root
    std::string ppath = to_physical_path(normalized_vpath);
    return (::rmdir(ppath.c_str()) == 0);
}

bool SandboxFs::delete_file(const std::string& normalized_vpath) {
    if (normalized_vpath == "/") return false;
    std::string ppath = to_physical_path(normalized_vpath);
    return (::unlink(ppath.c_str()) == 0);
}

bool SandboxFs::rename(const std::string& old_vpath, const std::string& new_vpath) {
    if (old_vpath == "/" || new_vpath == "/") return false;
    std::string old_ppath = to_physical_path(old_vpath);
    std::string new_ppath = to_physical_path(new_vpath);
    return (::rename(old_ppath.c_str(), new_ppath.c_str()) == 0);
}

std::string SandboxFs::format_unix_stat(const FileInfo& file) {
    std::ostringstream oss;
    oss << (file.is_directory ? 'd' : '-');
    oss << ((file.mode & S_IRUSR) ? 'r' : '-');
    oss << ((file.mode & S_IWUSR) ? 'w' : '-');
    oss << ((file.mode & S_IXUSR) ? 'x' : '-');
    oss << ((file.mode & S_IRGRP) ? 'r' : '-');
    oss << ((file.mode & S_IWGRP) ? 'w' : '-');
    oss << ((file.mode & S_IXGRP) ? 'x' : '-');
    oss << ((file.mode & S_IROTH) ? 'r' : '-');
    oss << ((file.mode & S_IWOTH) ? 'w' : '-');
    oss << ((file.mode & S_IXOTH) ? 'x' : '-');

    oss << "   1 ftp      ftp  ";
    oss << std::setw(12) << file.size << " ";

    struct tm tm_buf;
    localtime_r(&file.mtime, &tm_buf);
    char date_buf[64];
    std::strftime(date_buf, sizeof(date_buf), "%b %d %H:%M", &tm_buf);
    oss << date_buf << " " << file.name << "\r\n";

    return oss.str();
}

std::string SandboxFs::format_unix_list(const std::string& normalized_vdir) const {
    std::string ppath = to_physical_path(normalized_vdir);
    DIR* dir = ::opendir(ppath.c_str());
    if (!dir) return "";

    std::ostringstream oss;
    struct dirent* entry;
    std::vector<FileInfo> entries;

    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string item_vpath = (normalized_vdir == "/" ? "" : normalized_vdir) + "/" + name;
        FileInfo info;
        if (get_file_info(item_vpath, info)) {
            info.name = name;
            entries.push_back(info);
        }
    }
    ::closedir(dir);

    // Sort alphabetically
    std::sort(entries.begin(), entries.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.name < b.name;
    });

    for (const auto& item : entries) {
        oss << format_unix_stat(item);
    }
    return oss.str();
}

std::vector<std::string> SandboxFs::format_name_list(const std::string& normalized_vdir) const {
    std::vector<std::string> names;
    std::string ppath = to_physical_path(normalized_vdir);
    DIR* dir = ::opendir(ppath.c_str());
    if (!dir) return names;

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        names.push_back(name);
    }
    ::closedir(dir);
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace ftp
