#include "ftp_reply.hpp"
#include <sstream>

namespace ftp {

std::string FtpReply::full_message() const {
    if (lines_.empty()) return "";
    if (lines_.size() == 1) return lines_.front();

    std::ostringstream oss;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << lines_[i];
    }
    return oss.str();
}

std::string FtpReply::to_wire_string() const {
    if (lines_.empty()) {
        return std::to_string(code_) + " " + get_default_reply_text(static_cast<ReplyCode>(code_)) + "\r\n";
    }

    if (lines_.size() == 1) {
        return std::to_string(code_) + " " + lines_[0] + "\r\n";
    }

    std::ostringstream oss;
    oss << code_ << "-" << lines_[0] << "\r\n";
    for (size_t i = 1; i + 1 < lines_.size(); ++i) {
        oss << " " << lines_[i] << "\r\n";
    }
    oss << code_ << " " << lines_.back() << "\r\n";
    return oss.str();
}

bool FtpReply::parse(const std::string& buffer, FtpReply& out_reply, size_t& bytes_consumed) {
    bytes_consumed = 0;
    if (buffer.size() < 4) return false;

    // Check if starts with 3 digits
    if (!isdigit(buffer[0]) || !isdigit(buffer[1]) || !isdigit(buffer[2])) {
        return false;
    }

    int code = std::stoi(buffer.substr(0, 3));
    char separator = buffer[3];

    if (separator == ' ') {
        // Single line reply
        size_t eol = buffer.find("\r\n");
        if (eol == std::string::npos) {
            eol = buffer.find('\n');
            if (eol == std::string::npos) return false; // Incomplete line
            out_reply.code_ = code;
            out_reply.lines_ = { buffer.substr(4, eol - 4) };
            bytes_consumed = eol + 1;
            return true;
        }
        out_reply.code_ = code;
        out_reply.lines_ = { buffer.substr(4, eol - 4) };
        bytes_consumed = eol + 2;
        return true;
    } else if (separator == '-') {
        // Multiline reply: terminates when a line starts with code followed by space
        std::vector<std::string> lines;
        size_t pos = 0;
        std::string code_prefix = std::to_string(code) + " ";

        while (pos < buffer.size()) {
            size_t next_newline = buffer.find('\n', pos);
            if (next_newline == std::string::npos) return false; // Incomplete

            size_t line_len = next_newline - pos;
            std::string line = buffer.substr(pos, line_len);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (pos == 0) {
                // First line: <code>-<text>
                lines.push_back(line.substr(4));
            } else if (line.rfind(code_prefix, 0) == 0) {
                // Last line found!
                lines.push_back(line.substr(4));
                bytes_consumed = next_newline + 1;
                out_reply.code_ = code;
                out_reply.lines_ = std::move(lines);
                return true;
            } else {
                lines.push_back(line);
            }
            pos = next_newline + 1;
        }
        return false;
    }

    return false;
}

} // namespace ftp
