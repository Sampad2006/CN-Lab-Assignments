#include "ftp_client.hpp"
#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <csignal>

void print_help() {
    std::cout << ftp::colors::BOLD << "Available FTP Client commands:\n" << ftp::colors::RESET
              << "  open <host> [port]          Connect to FTP server (default port: " << ftp::DEFAULT_FTP_PORT << ")\n"
              << "  user <user> [pass]          Authenticate with credentials\n"
              << "  pwd                         Print current working directory on server\n"
              << "  cd <path>                   Change remote directory\n"
              << "  cdup                        Change to parent directory\n"
              << "  ls [path]                   List directory contents (standard long format)\n"
              << "  dir [path]                  Alias for 'ls'\n"
              << "  mkdir <dir>                 Create remote directory\n"
              << "  rmdir <dir>                 Remove remote directory\n"
              << "  get <remote> [local]        Download file from server\n"
              << "  put <local> [remote]        Upload file to server\n"
              << "  append <local> [remote]     Append file to remote file\n"
              << "  delete <file>               Delete remote file\n"
              << "  rename <old> <new>          Rename remote file or directory\n"
              << "  size <file>                 Get size of remote file\n"
              << "  mdtm <file>                 Get modification timestamp\n"
              << "  passive                     Toggle between Passive (PASV) and Active (PORT) mode\n"
              << "  binary / ascii              Set transfer mode (TYPE I vs TYPE A)\n"
              << "  resume <offset> <rem> [loc] Download resuming from byte offset\n"
              << "  close / disconnect          Close connection to server\n"
              << "  quit / bye / exit           Exit FTP client\n"
              << "  help                        Display this command manual\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);
    ftp::FtpClient client;

    std::cout << ftp::colors::CYAN << ftp::colors::BOLD
              << "====================================================================\n"
              << "       RFC 959 File Transfer Protocol (FTP) Interactive Client      \n"
              << "       CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University   \n"
              << "====================================================================\n"
              << ftp::colors::RESET
              << "Type 'help' for commands, or 'open <host> [port]' to connect.\n\n";

    if (argc >= 2) {
        std::string host = argv[1];
        uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::stoi(argv[2])) : ftp::DEFAULT_FTP_PORT;
        std::cout << "[*] Auto-connecting to " << host << ":" << port << "...\n";
        client.connect(host, port);
    }

    std::string line;
    while (true) {
        std::cout << ftp::colors::BOLD << "ftp> " << ftp::colors::RESET;
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::string trimmed = ftp::trim(line);
        if (trimmed.empty()) continue;

        std::istringstream iss(trimmed);
        std::string verb;
        iss >> verb;
        verb = ftp::to_upper(verb);

        if (verb == "QUIT" || verb == "BYE" || verb == "EXIT") {
            client.disconnect();
            std::cout << "Goodbye.\n";
            break;
        } else if (verb == "HELP") {
            print_help();
        } else if (verb == "OPEN") {
            std::string host;
            uint16_t port = ftp::DEFAULT_FTP_PORT;
            iss >> host;
            if (host.empty()) {
                std::cout << "Usage: open <host> [port]\n";
                continue;
            }
            if (iss >> port) {}
            if (client.connect(host, port)) {
                std::cout << "[+] Connected to " << host << ":" << port << "\n";
            } else {
                std::cout << "[-] Connection failed.\n";
            }
        } else if (verb == "CLOSE" || verb == "DISCONNECT") {
            client.disconnect();
            std::cout << "[*] Disconnected.\n";
        } else if (verb == "USER") {
            if (!client.is_connected()) {
                std::cout << "[*] Connection lost or not open. Connecting to 127.0.0.1:" << ftp::DEFAULT_FTP_PORT << "...\n";
                if (!client.connect("127.0.0.1", ftp::DEFAULT_FTP_PORT)) {
                    std::cout << "[-] Could not connect. Use 'open <host> [port]'.\n";
                    continue;
                }
            }
            std::string user, pass;
            iss >> user;
            if (user.empty()) {
                std::cout << "Usage: user <username> [password]\n";
                continue;
            }
            if (!(iss >> pass)) {
                std::cout << "Password: ";
                std::getline(std::cin, pass);
                pass = ftp::trim(pass);
            }
            if (client.login(user, pass)) {
                std::cout << "[+] Logged in successfully as " << user << ".\n";
            } else {
                std::cout << "[-] Login failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "PWD") {
            if (!client.is_connected()) {
                std::cout << "[-] Not connected. Use 'open <host> [port]' to connect.\n";
                continue;
            }
            std::string dir;
            if (client.pwd(dir)) {
                std::cout << "Remote directory: " << dir << "\n";
            } else {
                std::cout << "[-] PWD failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "CD" || verb == "CWD") {
            std::string dir;
            iss >> dir;
            if (dir.empty()) {
                std::cout << "Usage: cd <path>\n";
                continue;
            }
            if (client.cwd(dir)) {
                std::cout << "[+] Directory changed to " << dir << "\n";
            } else {
                std::cout << "[-] CWD failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "CDUP") {
            if (client.cdup()) {
                std::cout << "[+] Moved up to parent directory.\n";
            } else {
                std::cout << "[-] CDUP failed.\n";
            }
        } else if (verb == "LS" || verb == "DIR") {
            std::string path;
            iss >> path;
            std::string out;
            if (client.list(path, out)) {
                std::cout << out;
            } else {
                std::cout << "[-] Listing failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "MKDIR") {
            std::string dir;
            iss >> dir;
            if (dir.empty()) {
                std::cout << "Usage: mkdir <dir_name>\n";
                continue;
            }
            if (client.mkdir(dir)) {
                std::cout << "[+] Directory created: " << dir << "\n";
            } else {
                std::cout << "[-] MKD failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "RMDIR") {
            std::string dir;
            iss >> dir;
            if (dir.empty()) {
                std::cout << "Usage: rmdir <dir_name>\n";
                continue;
            }
            if (client.rmdir(dir)) {
                std::cout << "[+] Directory removed: " << dir << "\n";
            } else {
                std::cout << "[-] RMD failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "GET" || verb == "RECV") {
            std::string remote, local;
            iss >> remote;
            if (remote.empty()) {
                std::cout << "Usage: get <remote_file> [local_file]\n";
                continue;
            }
            if (!(iss >> local)) local = remote;
            std::cout << "[*] Downloading '" << remote << "' to '" << local << "'...\n";
            if (client.download_file(remote, local)) {
                std::cout << "[+] Download complete: " << local << "\n";
            } else {
                std::cout << "[-] Download failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "PUT" || verb == "SEND") {
            std::string local, remote;
            iss >> local;
            if (local.empty()) {
                std::cout << "Usage: put <local_file> [remote_file]\n";
                continue;
            }
            if (!(iss >> remote)) remote = local;
            std::cout << "[*] Uploading '" << local << "' to '" << remote << "'...\n";
            if (client.upload_file(local, remote)) {
                std::cout << "[+] Upload complete: " << remote << "\n";
            } else {
                std::cout << "[-] Upload failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "APPEND") {
            std::string local, remote;
            iss >> local >> remote;
            if (local.empty() || remote.empty()) {
                std::cout << "Usage: append <local_file> <remote_file>\n";
                continue;
            }
            if (client.upload_file(local, remote, true)) {
                std::cout << "[+] Appended successfully.\n";
            } else {
                std::cout << "[-] Append failed.\n";
            }
        } else if (verb == "DELETE" || verb == "RM") {
            std::string file;
            iss >> file;
            if (file.empty()) {
                std::cout << "Usage: delete <remote_file>\n";
                continue;
            }
            if (client.delete_file(file)) {
                std::cout << "[+] Deleted " << file << "\n";
            } else {
                std::cout << "[-] Delete failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "RENAME") {
            std::string old_n, new_n;
            iss >> old_n >> new_n;
            if (old_n.empty() || new_n.empty()) {
                std::cout << "Usage: rename <old_name> <new_name>\n";
                continue;
            }
            if (client.rename_file(old_n, new_n)) {
                std::cout << "[+] Renamed " << old_n << " to " << new_n << "\n";
            } else {
                std::cout << "[-] Rename failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "SIZE") {
            std::string file;
            iss >> file;
            uint64_t s = 0;
            if (client.get_size(file, s)) {
                std::cout << file << ": " << s << " bytes (" << ftp::format_bytes(s) << ")\n";
            } else {
                std::cout << "[-] SIZE failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "MDTM") {
            std::string file;
            iss >> file;
            std::string t;
            if (client.get_mod_time(file, t)) {
                std::cout << file << " mtime: " << t << "\n";
            } else {
                std::cout << "[-] MDTM failed: " << client.last_reply().full_message() << "\n";
            }
        } else if (verb == "PASSIVE") {
            bool next_mode = !client.is_passive();
            client.set_passive(next_mode);
            std::cout << "[*] Mode set to: " << (next_mode ? "PASSIVE (PASV)" : "ACTIVE (PORT)") << "\n";
        } else if (verb == "ACTIVE") {
            client.set_passive(false);
            std::cout << "[*] Mode set to: ACTIVE (PORT)\n";
        } else if (verb == "BINARY") {
            if (client.set_type(ftp::TransferType::BINARY_IMAGE)) {
                std::cout << "[*] Type set to Binary (IMAGE).\n";
            }
        } else if (verb == "ASCII") {
            if (client.set_type(ftp::TransferType::ASCII)) {
                std::cout << "[*] Type set to ASCII.\n";
            }
        } else if (verb == "RESUME") {
            uint64_t offset = 0;
            std::string remote, local;
            iss >> offset >> remote;
            if (!(iss >> local)) local = remote;
            if (remote.empty()) {
                std::cout << "Usage: resume <offset> <remote> [local]\n";
                continue;
            }
            std::cout << "[*] Resuming download of '" << remote << "' from offset " << offset << "...\n";
            if (client.download_file(remote, local, offset)) {
                std::cout << "[+] Resumed download complete.\n";
            } else {
                std::cout << "[-] Resume download failed: " << client.last_reply().full_message() << "\n";
            }
        } else {
            std::cout << "Unknown command: '" << verb << "'. Type 'help' for command list.\n";
        }
    }

    return 0;
}
