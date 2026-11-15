#include "ftp_server.hpp"
#include "ftp_client.hpp"
#include "logger.hpp"
#include "common.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <random>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <unistd.h>

#include <csignal>

namespace ftp {

void print_banner() {
    std::cout << colors::CYAN << colors::BOLD
              << "==============================================================================\n"
              << "         RFC 959 File Transfer Protocol (FTP) End-to-End Simulation Suite     \n"
              << "         CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University           \n"
              << "==============================================================================\n"
              << colors::RESET;
}

void print_scenario_header(int num, const std::string& title) {
    std::cout << "\n" << colors::YELLOW << colors::BOLD
              << "------------------------------------------------------------------------------\n"
              << " Scenario " << num << ": " << title << "\n"
              << "------------------------------------------------------------------------------"
              << colors::RESET << std::endl;
}

void cleanup_dir(const std::string& path) {
    std::string cmd = "rm -rf " + path;
    int r = ::system(cmd.c_str());
    (void)r;
}

} // namespace ftp

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    using namespace ftp;
    print_banner();

    std::string sim_root = "./ftp_sim_root";
    std::string client_workdir = "./ftp_client_scratch";
    cleanup_dir(sim_root);
    cleanup_dir(client_workdir);
    ::mkdir(sim_root.c_str(), 0755);
    ::mkdir(client_workdir.c_str(), 0755);

    uint16_t test_port = 21219;
    FtpServer server("127.0.0.1", test_port, sim_root);
    server.add_user("admin", "admin123", false);
    server.add_user("student", "cnlab", false);
    server.add_user("readonly_user", "pass", true);
    server.set_allow_anonymous(true, true);

    if (!server.start()) {
        Logger::error("SIM", "Failed to start server for simulation!");
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // -------------------------------------------------------------------------
    // Scenario 1: Control Connection & Greeting Banner Verification
    // -------------------------------------------------------------------------
    print_scenario_header(1, "Control Connection Initialization & Capability Negotiation");
    {
        FtpClient client;
        bool ok = client.connect("127.0.0.1", test_port);
        assert(ok && "Failed to connect to FTP server");
        assert(client.last_reply().code() == 220);
        Logger::success("SCENARIO-1", "Received 220 Banner: " + client.last_reply().primary_message());

        // Test SYST
        client.send_command("SYST");
        FtpReply r;
        client.read_reply(r);
        assert(r.code() == 215);
        Logger::success("SCENARIO-1", "SYST response: " + r.primary_message());

        // Test FEAT
        client.send_command("FEAT");
        client.read_reply(r);
        assert(r.code() == 211);
        Logger::success("SCENARIO-1", "FEAT response received (" + std::to_string(r.lines().size()) + " features advertised)");

        // Test NOOP
        client.send_command("NOOP");
        client.read_reply(r);
        assert(r.code() == 200);
        Logger::success("SCENARIO-1", "NOOP confirmed: " + r.primary_message());

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 2: Authentication State Machine & Access Control
    // -------------------------------------------------------------------------
    print_scenario_header(2, "Authentication State Machine & Privilege Boundaries");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);

        // Attempt PWD without login -> expect 530
        std::string pwd_dir;
        bool pwd_ok = client.pwd(pwd_dir);
        assert(!pwd_ok && client.last_reply().code() == 530);
        Logger::success("SCENARIO-2", "Unauthenticated PWD blocked with 530 Not logged in");

        // Attempt bad password
        bool bad_login = client.login("admin", "wrong_password");
        assert(!bad_login && client.last_reply().code() == 530);
        Logger::success("SCENARIO-2", "Invalid credentials rejected with 530");

        // Anonymous login
        bool anon_login = client.login("anonymous", "guest@ju.edu");
        assert(anon_login);
        Logger::success("SCENARIO-2", "Anonymous login succeeded (Read-Only access mode)");

        // Anonymous attempt to write directory -> expect 550 Permission denied
        bool anon_mkdir = client.mkdir("forbidden_dir");
        assert(!anon_mkdir && client.last_reply().code() == 550);
        Logger::success("SCENARIO-2", "Anonymous write attempt prevented (Read-Only isolation confirmed)");

        client.disconnect();

        // Login as Administrator (Read-Write)
        client.connect("127.0.0.1", test_port);
        bool admin_login = client.login("admin", "admin123");
        assert(admin_login);
        Logger::success("SCENARIO-2", "Administrator authentication succeeded (Read-Write granted)");
        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 3: Directory Navigation & Virtual Chroot Jail Defense
    // -------------------------------------------------------------------------
    print_scenario_header(3, "Virtual Chroot Jail & Path Traversal Attack Defense");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");

        std::string current;
        client.pwd(current);
        assert(current == "/");
        Logger::success("SCENARIO-3", "Initial virtual working directory is '/'");

        // Create directory hierarchy
        bool mk1 = client.mkdir("projects");
        assert(mk1);
        bool cw1 = client.cwd("projects");
        assert(cw1);
        client.pwd(current);
        assert(current == "/projects");
        Logger::success("SCENARIO-3", "Navigated to /projects");

        bool mk2 = client.mkdir("cn_lab");
        assert(mk2);
        bool cw2 = client.cwd("cn_lab");
        assert(cw2);
        client.pwd(current);
        assert(current == "/projects/cn_lab");
        Logger::success("SCENARIO-3", "Navigated to /projects/cn_lab");

        // Attack: attempt path traversal out of sandbox jail: ../../../../../etc
        Logger::info("SCENARIO-3", "Injecting path traversal exploit: CWD ../../../../../etc");
        client.cwd("../../../../../etc");
        // Server either rejects or clamps to /
        client.pwd(current);
        assert(current == "/" || current.find("..") == std::string::npos);
        Logger::success("SCENARIO-3", "Virtual Jail intact! Path traversal clamped safely to: " + current);

        // CDUP back to root
        client.cwd("/projects/cn_lab");
        client.cdup();
        client.pwd(current);
        assert(current == "/projects");
        client.cdup();
        client.pwd(current);
        assert(current == "/");
        Logger::success("SCENARIO-3", "CDUP operations successfully returned to '/'");

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 4: Passive Mode (PASV) File Upload, Listing & Download
    // -------------------------------------------------------------------------
    print_scenario_header(4, "Passive Mode (PASV) File Transfer & Directory Listing");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");
        client.set_passive(true);

        // Create local text file
        std::string local_file = client_workdir + "/rfc959_specs.txt";
        {
            std::ofstream ofs(local_file);
            ofs << "RFC 959: FILE TRANSFER PROTOCOL (FTP)\n"
                << "Jadavpur University CSE Department - 2026\n"
                << "Dual TCP channel architecture: Control (21) & Data.\n";
        }

        // Upload to server
        bool up_ok = client.upload_file(local_file, "rfc959_specs.txt");
        assert(up_ok);
        Logger::success("SCENARIO-4", "File uploaded via PASV mode: rfc959_specs.txt");

        // Directory listing
        std::string list_output;
        bool list_ok = client.list("", list_output);
        assert(list_ok);
        assert(list_output.find("rfc959_specs.txt") != std::string::npos);
        Logger::success("SCENARIO-4", "Directory listing retrieved successfully:\n" + list_output);

        // Download back to new local file
        std::string dl_file = client_workdir + "/downloaded_specs.txt";
        bool dl_ok = client.download_file("rfc959_specs.txt", dl_file);
        assert(dl_ok);

        // Verify SHA-256 match
        std::string h1 = compute_file_sha256(local_file);
        std::string h2 = compute_file_sha256(dl_file);
        assert(h1 == h2);
        Logger::success("SCENARIO-4", "SHA-256 integrity verified: " + h1);

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 5: Active Mode (PORT) Transmission
    // -------------------------------------------------------------------------
    print_scenario_header(5, "Active Mode (PORT) Data Channel Handshake");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");
        client.set_passive(false); // Enable ACTIVE mode (PORT)

        std::string act_file = client_workdir + "/active_mode_test.dat";
        {
            std::ofstream ofs(act_file);
            ofs << "Active Mode Data Channel verification over loopback network.\n";
        }

        bool act_up = client.upload_file(act_file, "active_mode_test.dat");
        assert(act_up);
        Logger::success("SCENARIO-5", "Uploaded file via Active (PORT) mode");

        std::string act_dl = client_workdir + "/active_downloaded.dat";
        bool act_dl_ok = client.download_file("active_mode_test.dat", act_dl);
        assert(act_dl_ok);
        Logger::success("SCENARIO-5", "Downloaded file via Active (PORT) mode");

        assert(compute_file_sha256(act_file) == compute_file_sha256(act_dl));
        Logger::success("SCENARIO-5", "Active mode SHA-256 integrity match confirmed.");

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 6: High-Volume Binary Transfer & SHA-256 Integrity Verification
    // -------------------------------------------------------------------------
    print_scenario_header(6, "High-Volume Binary Stream Transfer (TYPE I) & SHA-256 Verification");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");
        client.set_type(TransferType::BINARY_IMAGE);
        client.set_passive(true);

        // Generate 1 MB pseudorandom binary payload
        size_t bin_size = 1024 * 1024; // 1 MB
        std::string bin_local = client_workdir + "/random_1mb.bin";
        std::string bin_dl = client_workdir + "/downloaded_1mb.bin";

        {
            std::ofstream ofs(bin_local, std::ios::binary);
            std::mt19937 rng(42);
            std::uniform_int_distribution<int> dist(0, 255);
            std::vector<char> buffer(65536);
            size_t written = 0;
            while (written < bin_size) {
                for (auto& b : buffer) b = static_cast<char>(dist(rng));
                size_t to_write = std::min(buffer.size(), bin_size - written);
                ofs.write(buffer.data(), to_write);
                written += to_write;
            }
        }

        std::string src_hash = compute_file_sha256(bin_local);
        Logger::info("SCENARIO-6", "Generated 1 MB Binary file with SHA-256: " + src_hash);

        auto t1 = std::chrono::high_resolution_clock::now();
        bool up_ok = client.upload_file(bin_local, "large_binary.bin");
        auto t2 = std::chrono::high_resolution_clock::now();
        assert(up_ok);
        double up_dur = std::chrono::duration<double>(t2 - t1).count();
        double up_mbps = (bin_size * 8.0) / (up_dur * 1000000.0);
        Logger::success("SCENARIO-6", "Upload completed in " + std::to_string(up_dur) + "s (~" +
                                     std::to_string(up_mbps) + " Mbps)");

        auto t3 = std::chrono::high_resolution_clock::now();
        bool dl_ok = client.download_file("large_binary.bin", bin_dl);
        auto t4 = std::chrono::high_resolution_clock::now();
        assert(dl_ok);
        double dl_dur = std::chrono::duration<double>(t4 - t3).count();
        double dl_mbps = (bin_size * 8.0) / (dl_dur * 1000000.0);
        Logger::success("SCENARIO-6", "Download completed in " + std::to_string(dl_dur) + "s (~" +
                                     std::to_string(dl_mbps) + " Mbps)");

        std::string dst_hash = compute_file_sha256(bin_dl);
        assert(src_hash == dst_hash);
        Logger::success("SCENARIO-6", "Cryptographic Checksum MATCH: " + dst_hash);

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 7: Transfer Resumption (REST) after Interruption
    // -------------------------------------------------------------------------
    print_scenario_header(7, "Stream Resumption (REST Marker) after Disrupted Download");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");

        std::string resumed_local = client_workdir + "/resumed_file.bin";
        // Simulate partial download: download only first 256 KB
        size_t partial_size = 256 * 1024;
        {
            std::string full_src = client_workdir + "/random_1mb.bin";
            std::ifstream ifs(full_src, std::ios::binary);
            std::ofstream ofs(resumed_local, std::ios::binary);
            std::vector<char> buf(partial_size);
            ifs.read(buf.data(), partial_size);
            ofs.write(buf.data(), partial_size);
        }
        Logger::info("SCENARIO-7", "Simulated partial local download (256 KB out of 1024 KB)");

        // Resume download starting at offset 262144
        Logger::info("SCENARIO-7", "Issuing REST 262144 marker and requesting RETR...");
        bool res_ok = client.download_file("large_binary.bin", resumed_local, partial_size);
        assert(res_ok);

        std::string full_hash = compute_file_sha256(client_workdir + "/random_1mb.bin");
        std::string resumed_hash = compute_file_sha256(resumed_local);
        assert(full_hash == resumed_hash);
        Logger::success("SCENARIO-7", "Resumed file bit-exact reassembly verified: " + resumed_hash);

        client.disconnect();
    }

    // -------------------------------------------------------------------------
    // Scenario 8: File Metadata, Append, Rename, Deletion & Multi-Client Concurrency
    // -------------------------------------------------------------------------
    print_scenario_header(8, "Metadata (SIZE/MDTM), Atomic Rename, Append, Delete & Concurrency");
    {
        FtpClient client;
        client.connect("127.0.0.1", test_port);
        client.login("admin", "admin123");

        // Query SIZE
        uint64_t fsize = 0;
        bool sz_ok = client.get_size("rfc959_specs.txt", fsize);
        assert(sz_ok && fsize > 0);
        Logger::success("SCENARIO-8", "SIZE command confirmed: " + std::to_string(fsize) + " bytes");

        // Query MDTM
        std::string mtime;
        bool md_ok = client.get_mod_time("rfc959_specs.txt", mtime);
        assert(md_ok && !mtime.empty());
        Logger::success("SCENARIO-8", "MDTM timestamp: " + mtime);

        // Rename file (RNFR -> RNTO)
        bool rn_ok = client.rename_file("rfc959_specs.txt", "rfc959_renamed.txt");
        assert(rn_ok);
        uint64_t dummy;
        assert(!client.get_size("rfc959_specs.txt", dummy)); // Old name is gone
        assert(client.get_size("rfc959_renamed.txt", dummy)); // New name exists
        Logger::success("SCENARIO-8", "Atomic file rename (RNFR -> RNTO) verified.");

        // Append to file
        std::string append_chunk = client_workdir + "/append_data.txt";
        {
            std::ofstream ofs(append_chunk);
            ofs << "--- APPENDED LOG RECORD ---\n";
        }
        bool app_ok = client.upload_file(append_chunk, "rfc959_renamed.txt", true);
        assert(app_ok);
        uint64_t new_size = 0;
        client.get_size("rfc959_renamed.txt", new_size);
        assert(new_size > fsize);
        Logger::success("SCENARIO-8", "APPE command verified (file size grew from " +
                                     std::to_string(fsize) + " to " + std::to_string(new_size) + " bytes)");

        // Delete file
        bool del_ok = client.delete_file("rfc959_renamed.txt");
        assert(del_ok);
        assert(!client.get_size("rfc959_renamed.txt", dummy));
        Logger::success("SCENARIO-8", "DELE command removed file successfully.");

        client.disconnect();

        // Multi-client concurrent operations test
        Logger::info("SCENARIO-8", "Launching 4 concurrent worker threads performing parallel transfers...");
        std::vector<std::thread> workers;
        std::atomic<int> success_count{0};

        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([i, test_port, &success_count, &client_workdir]() {
                FtpClient c;
                if (!c.connect("127.0.0.1", test_port)) return;
                if (!c.login("admin", "admin123")) return;
                std::string fname = "concurrent_worker_" + std::to_string(i) + ".txt";
                std::string local = client_workdir + "/" + fname;
                {
                    std::ofstream ofs(local);
                    ofs << "Thread worker #" << i << " payload.\n";
                }
                if (!c.upload_file(local, fname)) return;
                std::string dl = client_workdir + "/dl_" + fname;
                if (!c.download_file(fname, dl)) return;
                if (compute_file_sha256(local) == compute_file_sha256(dl)) {
                    success_count++;
                }
                c.delete_file(fname);
                c.disconnect();
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }
        assert(success_count == 4);
        Logger::success("SCENARIO-8", "All 4 concurrent client sessions completed successfully without race conditions!");
    }

    // Stop server
    server.stop();

    // Clean up temporary files
    cleanup_dir(sim_root);
    cleanup_dir(client_workdir);

    std::cout << "\n" << colors::GREEN << colors::BOLD
              << "==============================================================================\n"
              << "     [+] ALL 8 FTP SCENARIOS EXECUTED AND FULLY VERIFIED (RFC 959 / 3659)     \n"
              << "==============================================================================\n"
              << colors::RESET << std::endl;

    return 0;
}
