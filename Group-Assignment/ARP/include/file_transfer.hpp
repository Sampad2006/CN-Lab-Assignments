#ifndef ARP_FILE_TRANSFER_HPP
#define ARP_FILE_TRANSFER_HPP

#include "ethernet.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>

namespace arp {

constexpr size_t FILE_CHUNK_SIZE = 1024; // 1 KB chunks

enum class FileMsgType : uint8_t {
    START = 1,
    DATA  = 2,
    END   = 3
};

#pragma pack(push, 1)
struct FilePacketHeader {
    uint8_t  msg_type;       // FileMsgType
    uint32_t transfer_id;    // Unique transfer identifier
    uint32_t chunk_idx;      // 0-based chunk index
    uint32_t total_chunks;   // Total number of chunks
    uint32_t chunk_size;     // Size of current chunk payload
    uint32_t file_size;      // Total file size in bytes
    char     filename[128];  // Basename of the file
};
#pragma pack(pop)

struct FileTransferSession {
    uint32_t transfer_id{0};
    std::string filename;
    uint32_t file_size{0};
    uint32_t total_chunks{0};
    uint32_t received_chunks{0};
    std::vector<std::vector<uint8_t>> chunks;
    std::vector<bool> chunk_received;

    bool is_complete() const {
        return total_chunks > 0 && received_chunks == total_chunks;
    }
};

} // namespace arp

#endif // ARP_FILE_TRANSFER_HPP
