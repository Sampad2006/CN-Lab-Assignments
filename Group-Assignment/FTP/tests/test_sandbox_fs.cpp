#include "sandbox_fs.hpp"
#include <iostream>
#include <cassert>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    using namespace ftp;
    std::cout << "[*] Running Test: Sandbox Virtual Filesystem & Path Traversal Prevention..." << std::endl;

    std::string test_dir = "./test_sandbox_root";
    ::system(("rm -rf " + test_dir).c_str());
    ::mkdir(test_dir.c_str(), 0755);

    SandboxFs fs(test_dir);

    // Test 1: Path normalization and directory traversal defense
    {
        std::string cur = "/";
        assert(fs.normalize_virtual_path("folder", cur) == "/folder");
        assert(fs.normalize_virtual_path("/folder", cur) == "/folder");
        assert(fs.normalize_virtual_path("folder/sub", cur) == "/folder/sub");

        // Traversal attempts
        assert(fs.normalize_virtual_path("..", cur) == "/");
        assert(fs.normalize_virtual_path("../..", cur) == "/");
        assert(fs.normalize_virtual_path("../../../etc/shadow", cur) == "/etc/shadow");
        // Verify physical path stays prefixed with test_dir
        std::string phys = fs.to_physical_path(fs.normalize_virtual_path("../../../etc/shadow", cur));
        assert(phys == test_dir + "/etc/shadow");

        cur = "/projects/alpha";
        assert(fs.normalize_virtual_path("..", cur) == "/projects");
        assert(fs.normalize_virtual_path("../..", cur) == "/");
        assert(fs.normalize_virtual_path("../../..", cur) == "/");
        assert(fs.normalize_virtual_path("beta/../gamma", cur) == "/projects/alpha/gamma");
    }

    // Test 2: Directory creation, listing, file creation, deletion
    {
        assert(fs.make_directory("/docs"));
        assert(fs.exists("/docs"));
        assert(fs.is_directory("/docs"));

        assert(fs.make_directory("/docs/reports"));
        assert(fs.exists("/docs/reports"));

        // Format name list
        auto nlist = fs.format_name_list("/docs");
        assert(nlist.size() == 1);
        assert(nlist[0] == "reports");

        // Rename
        assert(fs.rename("/docs/reports", "/docs/final_reports"));
        assert(!fs.exists("/docs/reports"));
        assert(fs.exists("/docs/final_reports"));

        // Remove dir
        assert(fs.remove_directory("/docs/final_reports"));
        assert(!fs.exists("/docs/final_reports"));

        assert(fs.remove_directory("/docs"));
        assert(!fs.exists("/docs"));
    }

    ::system(("rm -rf " + test_dir).c_str());
    std::cout << "[+] test_sandbox_fs passed successfully!" << std::endl;
    return 0;
}
