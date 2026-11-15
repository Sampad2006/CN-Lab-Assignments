#ifndef FTP_REPLY_HPP
#define FTP_REPLY_HPP

#include <string>
#include <vector>
#include "common.hpp"

namespace ftp {

class FtpReply {
public:
    FtpReply() : code_(0) {}
    FtpReply(int code, std::string text) : code_(code), lines_({std::move(text)}) {}
    FtpReply(ReplyCode code, std::string text) : code_(static_cast<int>(code)), lines_({std::move(text)}) {}
    FtpReply(int code, std::vector<std::string> lines) : code_(code), lines_(std::move(lines)) {}

    int code() const { return code_; }
    bool is_positive_preliminary() const { return code_ >= 100 && code_ < 200; }
    bool is_positive_completion() const { return code_ >= 200 && code_ < 300; }
    bool is_positive_intermediate() const { return code_ >= 300 && code_ < 400; }
    bool is_transient_negative() const { return code_ >= 400 && code_ < 500; }
    bool is_permanent_negative() const { return code_ >= 500 && code_ < 600; }
    bool is_success() const { return is_positive_completion() || is_positive_preliminary() || is_positive_intermediate(); }

    const std::vector<std::string>& lines() const { return lines_; }
    std::string primary_message() const { return lines_.empty() ? "" : lines_.front(); }
    std::string full_message() const;

    // Serialize to RFC 959 compliant wire format
    std::string to_wire_string() const;

    // Parse single or multiline replies from a socket or buffer
    static bool parse(const std::string& buffer, FtpReply& out_reply, size_t& bytes_consumed);

private:
    int code_;
    std::vector<std::string> lines_;
};

} // namespace ftp

#endif // FTP_REPLY_HPP
