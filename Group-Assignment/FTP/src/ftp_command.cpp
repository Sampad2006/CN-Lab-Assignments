#include "ftp_command.hpp"
#include "common.hpp"

namespace ftp {

bool FtpCommand::is(const std::string& v) const {
    return verb_ == to_upper(v);
}

std::string FtpCommand::to_wire_string() const {
    if (argument_.empty()) {
        return verb_ + "\r\n";
    }
    return verb_ + " " + argument_ + "\r\n";
}

bool FtpCommand::parse(const std::string& raw_line, FtpCommand& out_cmd) {
    std::string s = trim(raw_line);
    if (s.empty()) return false;

    size_t space_idx = s.find(' ');
    if (space_idx == std::string::npos) {
        out_cmd.verb_ = to_upper(s);
        out_cmd.argument_.clear();
    } else {
        out_cmd.verb_ = to_upper(s.substr(0, space_idx));
        std::string arg = s.substr(space_idx + 1);
        // Trim leading spaces from argument, but preserve internal/trailing as filename might need
        size_t first_non_space = arg.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) {
            out_cmd.argument_ = arg.substr(first_non_space);
        } else {
            out_cmd.argument_.clear();
        }
    }
    return !out_cmd.verb_.empty();
}

} // namespace ftp
