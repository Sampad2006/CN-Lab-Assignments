#ifndef FTP_LOGGER_HPP
#define FTP_LOGGER_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace ftp {

namespace colors {
    constexpr const char* RESET       = "\033[0m";
    constexpr const char* RED         = "\033[31m";
    constexpr const char* GREEN       = "\033[32m";
    constexpr const char* YELLOW      = "\033[33m";
    constexpr const char* BLUE        = "\033[34m";
    constexpr const char* MAGENTA     = "\033[35m";
    constexpr const char* CYAN        = "\033[36m";
    constexpr const char* WHITE       = "\033[37m";
    constexpr const char* BOLD        = "\033[1m";
    constexpr const char* DIM         = "\033[2m";
    constexpr const char* BG_BLUE     = "\033[44m";
    constexpr const char* BG_GREEN    = "\033[42m";
}

class Logger {
private:
    static inline std::mutex log_mutex;

    static std::string timestamp_str() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

public:
    static void info(const std::string& tag, const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::CYAN << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void success(const std::string& tag, const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::GREEN << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void warn(const std::string& tag, const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::YELLOW << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void error(const std::string& tag, const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::RED << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void command(const std::string& session, const std::string& cmd_line) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::BLUE << colors::BOLD << "[CMD " << session << "] " << colors::RESET
                  << colors::BOLD << "-> " << cmd_line << colors::RESET << std::endl;
    }

    static void reply(const std::string& session, int code, const std::string& reply_text) {
        std::lock_guard<std::mutex> lock(log_mutex);
        const char* code_color = colors::GREEN;
        if (code >= 400 && code < 500) code_color = colors::YELLOW;
        else if (code >= 500) code_color = colors::RED;
        else if (code >= 100 && code < 200) code_color = colors::CYAN;

        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::MAGENTA << colors::BOLD << "[RPL " << session << "] " << colors::RESET
                  << code_color << colors::BOLD << "<- " << code << " " << reply_text << colors::RESET << std::endl;
    }

    static void data(const std::string& session, const std::string& direction, size_t bytes, const std::string& desc = "") {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::CYAN << colors::BOLD << "[DATA " << session << "] " << colors::RESET
                  << "[" << direction << "] Transferred " << bytes << " bytes "
                  << (desc.empty() ? "" : "(" + desc + ")") << std::endl;
    }
};

} // namespace ftp

#endif // FTP_LOGGER_HPP
