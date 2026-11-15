#ifndef FTP_COMMAND_HPP
#define FTP_COMMAND_HPP

#include <string>
#include <vector>

namespace ftp {

class FtpCommand {
public:
    FtpCommand() = default;
    FtpCommand(std::string verb, std::string arg)
        : verb_(std::move(verb)), argument_(std::move(arg)) {}

    const std::string& verb() const { return verb_; }
    const std::string& argument() const { return argument_; }

    bool is(const std::string& v) const;

    // Serialize to FTP command wire format: "<VERB> <ARG>\r\n"
    std::string to_wire_string() const;

    // Parse an incoming raw command line string from control socket
    static bool parse(const std::string& raw_line, FtpCommand& out_cmd);

private:
    std::string verb_;     // Uppercase command name (e.g., "USER", "PASV", "RETR")
    std::string argument_; // Preserved case arguments (e.g., path/filename)
};

} // namespace ftp

#endif // FTP_COMMAND_HPP
