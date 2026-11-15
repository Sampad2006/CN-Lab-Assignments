#include "common.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <vector>
#include <unistd.h>

int main() {
    using namespace ftp;
    std::cout << "[*] Running Test: SHA-256 Checksum & Transfer Integrity..." << std::endl;

    // Test standard NIST / FIPS 180-4 test vectors
    {
        // Vector 1: Empty string ""
        std::string h_empty = compute_data_sha256("", 0);
        assert(h_empty == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        // Vector 2: "abc"
        std::string h_abc = compute_data_sha256("abc", 3);
        assert(h_abc == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

        // Vector 3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
        std::string msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        std::string h_msg = compute_data_sha256(msg.data(), msg.size());
        assert(h_msg == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    // Test file chunk hash matching
    {
        std::string test_file = "./test_int.tmp";
        std::string data = "The quick brown fox jumps over the lazy dog";
        {
            std::ofstream ofs(test_file, std::ios::binary);
            ofs << data;
        }

        std::string file_hash = compute_file_sha256(test_file);
        std::string data_hash = compute_data_sha256(data.data(), data.size());
        assert(file_hash == data_hash);
        assert(file_hash == "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");

        ::unlink(test_file.c_str());
    }

    std::cout << "[+] test_transfer_integrity passed successfully!" << std::endl;
    return 0;
}
