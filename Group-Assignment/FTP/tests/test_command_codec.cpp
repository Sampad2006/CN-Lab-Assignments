#include "ftp_command.hpp"
#include "ftp_reply.hpp"
#include "common.hpp"
#include <iostream>
#include <cassert>

int main() {
    using namespace ftp;
    std::cout << "[*] Running Test: FTP Command & Reply Codec..." << std::endl;

    // Test 1: Command parsing
    {
        FtpCommand cmd;
        assert(FtpCommand::parse("user anonymous\r\n", cmd));
        assert(cmd.is("USER"));
        assert(cmd.argument() == "anonymous");

        assert(FtpCommand::parse("PASS secret123", cmd));
        assert(cmd.is("PASS"));
        assert(cmd.argument() == "secret123");

        assert(FtpCommand::parse("PASV", cmd));
        assert(cmd.is("PASV"));
        assert(cmd.argument().empty());

        assert(FtpCommand::parse("retr   /sub/my file.txt\r\n", cmd));
        assert(cmd.is("RETR"));
        assert(cmd.argument() == "/sub/my file.txt");
    }

    // Test 2: Reply single line parsing
    {
        FtpReply r;
        size_t consumed = 0;
        std::string raw = "220 Welcome to FTP Server\r\n";
        assert(FtpReply::parse(raw, r, consumed));
        assert(r.code() == 220);
        assert(r.primary_message() == "Welcome to FTP Server");
        assert(consumed == raw.size());
        assert(r.is_positive_completion());
    }

    // Test 3: Reply multiline parsing
    {
        FtpReply r;
        size_t consumed = 0;
        std::string raw = "211-Features supported:\r\n MDTM\r\n SIZE\r\n211 End of features\r\n";
        assert(FtpReply::parse(raw, r, consumed));
        assert(r.code() == 211);
        assert(r.lines().size() == 4);
        assert(r.lines()[0] == "Features supported:");
        assert(r.lines().back() == "End of features");
        assert(consumed == raw.size());
    }

    // Test 4: IpPortTuple PORT formatting & parsing
    {
        IpPortTuple t("192.168.1.50", 5000);
        std::string comma = t.to_comma_format();
        // 5000 = (19 << 8) | 136 -> 19, 136
        assert(comma == "192,168,1,50,19,136");

        IpPortTuple parsed;
        assert(IpPortTuple::from_comma_format(comma, parsed));
        assert(parsed.ip == "192.168.1.50");
        assert(parsed.port == 5000);

        // PASV reply parsing
        std::string pasv_reply = "227 Entering Passive Mode (10,0,0,1,8,73).";
        IpPortTuple pasv_out;
        assert(IpPortTuple::from_pasv_reply(pasv_reply, pasv_out));
        assert(pasv_out.ip == "10.0.0.1");
        assert(pasv_out.port == (8 * 256 + 73));
    }

    std::cout << "[+] test_command_codec passed successfully!" << std::endl;
    return 0;
}
