#ifndef ARP_LOGGER_HPP
#define ARP_LOGGER_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace arp {

namespace colors {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* BOLD    = "\033[1m";
    constexpr const char* DIM     = "\033[2m";
}

class Logger {
public:
    static std::string timestamp_str() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    static void info(const std::string& tag, const std::string& msg) {
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::CYAN << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void success(const std::string& tag, const std::string& msg) {
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::GREEN << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void warn(const std::string& tag, const std::string& msg) {
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::YELLOW << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void error(const std::string& tag, const std::string& msg) {
        std::cerr << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << colors::RED << colors::BOLD << "[" << tag << "] " << colors::RESET
                  << msg << std::endl;
    }

    static void packet(const std::string& direction, const std::string& summary) {
        const char* col = (direction == "TX" || direction == "SEND") ? colors::BLUE : colors::MAGENTA;
        std::cout << colors::DIM << "[" << timestamp_str() << "] " << colors::RESET
                  << col << colors::BOLD << "[" << direction << "] " << colors::RESET
                  << summary << std::endl;
    }
};

} // namespace arp

#endif // ARP_LOGGER_HPP
