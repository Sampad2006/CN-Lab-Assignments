#include "common.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>

namespace ftp {

std::string IpPortTuple::to_comma_format() const {
    std::vector<std::string> parts = split(ip, '.');
    if (parts.size() != 4) {
        return "127,0,0,1,0,0";
    }
    uint8_t p1 = static_cast<uint8_t>((port >> 8) & 0xFF);
    uint8_t p2 = static_cast<uint8_t>(port & 0xFF);
    std::ostringstream oss;
    oss << parts[0] << "," << parts[1] << "," << parts[2] << "," << parts[3]
        << "," << static_cast<int>(p1) << "," << static_cast<int>(p2);
    return oss.str();
}

std::string IpPortTuple::to_string() const {
    return ip + ":" + std::to_string(port);
}

bool IpPortTuple::from_comma_format(const std::string& str, IpPortTuple& out) {
    std::vector<std::string> parts = split(trim(str), ',');
    if (parts.size() != 6) return false;

    try {
        int h1 = std::stoi(parts[0]);
        int h2 = std::stoi(parts[1]);
        int h3 = std::stoi(parts[2]);
        int h4 = std::stoi(parts[3]);
        int p1 = std::stoi(parts[4]);
        int p2 = std::stoi(parts[5]);

        if (h1 < 0 || h1 > 255 || h2 < 0 || h2 > 255 ||
            h3 < 0 || h3 > 255 || h4 < 0 || h4 > 255 ||
            p1 < 0 || p1 > 255 || p2 < 0 || p2 > 255) {
            return false;
        }

        out.ip = std::to_string(h1) + "." + std::to_string(h2) + "." +
                 std::to_string(h3) + "." + std::to_string(h4);
        out.port = static_cast<uint16_t>((p1 << 8) | p2);
        return true;
    } catch (...) {
        return false;
    }
}

bool IpPortTuple::from_pasv_reply(const std::string& reply_text, IpPortTuple& out) {
    // Reply usually format: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
    size_t open_paren = reply_text.find('(');
    size_t close_paren = reply_text.find(')', open_paren);
    if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren) {
        // Try fallback searching for 6 comma-separated numbers
        return from_comma_format(reply_text, out);
    }
    std::string inner = reply_text.substr(open_paren + 1, close_paren - open_paren - 1);
    return from_comma_format(inner, out);
}

bool IpPortTuple::from_epsv_reply(const std::string& reply_text, uint16_t& out_port) {
    // RFC 2428: 229 Entering Extended Passive Mode (|||port|)
    size_t open_paren = reply_text.find('(');
    size_t close_paren = reply_text.find(')', open_paren);
    if (open_paren == std::string::npos || close_paren == std::string::npos) return false;
    std::string inner = reply_text.substr(open_paren + 1, close_paren - open_paren - 1);
    // Find delimiter which is usually '|'
    if (inner.size() < 4) return false;
    char delim = inner[0];
    auto parts = split(inner, delim);
    // e.g. "" "" "" "port" ""
    for (const auto& p : parts) {
        if (!p.empty()) {
            try {
                int port = std::stoi(p);
                if (port > 0 && port <= 65535) {
                    out_port = static_cast<uint16_t>(port);
                    return true;
                }
            } catch (...) {}
        }
    }
    return false;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_upper(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return res;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double d_bytes = static_cast<double>(bytes);
    while (d_bytes >= 1024.0 && unit_idx < 4) {
        d_bytes /= 1024.0;
        unit_idx++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(unit_idx == 0 ? 0 : 2) << d_bytes << " " << units[unit_idx];
    return oss.str();
}

std::string get_default_reply_text(ReplyCode code) {
    switch (code) {
        case ReplyCode::RESTART_MARKER: return "Restart marker reply.";
        case ReplyCode::SERVICE_READY_MINUTES: return "Service ready in n minutes.";
        case ReplyCode::DATA_CONN_ALREADY_OPEN: return "Data connection already open; transfer starting.";
        case ReplyCode::FILE_STATUS_OK_OPEN_DATA: return "File status okay; about to open data connection.";
        case ReplyCode::COMMAND_OK: return "Command okay.";
        case ReplyCode::COMMAND_NOT_IMPLEMENTED_SUP: return "Command not implemented, superfluous at this site.";
        case ReplyCode::SYSTEM_STATUS: return "System status, or system help reply.";
        case ReplyCode::DIRECTORY_STATUS: return "Directory status.";
        case ReplyCode::FILE_STATUS: return "File status.";
        case ReplyCode::HELP_MESSAGE: return "Help message.";
        case ReplyCode::SYSTEM_NAME_TYPE: return "UNIX Type: L8";
        case ReplyCode::SERVICE_READY: return "JU FTP Server (RFC 959) ready.";
        case ReplyCode::CLOSING_CONTROL_CONN: return "Goodbye.";
        case ReplyCode::DATA_CONN_OPEN_NO_TRANSFER: return "Data connection open; no transfer in progress.";
        case ReplyCode::CLOSING_DATA_CONN_SUCCESS: return "Closing data connection. Requested file action successful.";
        case ReplyCode::ENTERING_PASSIVE_MODE: return "Entering Passive Mode.";
        case ReplyCode::ENTERING_EXTENDED_PASSIVE: return "Entering Extended Passive Mode.";
        case ReplyCode::USER_LOGGED_IN: return "User logged in, proceed.";
        case ReplyCode::REQUESTED_FILE_ACTION_OK: return "Requested file action okay, completed.";
        case ReplyCode::PATHNAME_CREATED: return "PATHNAME created.";
        case ReplyCode::USER_NAME_OK_NEED_PASS: return "User name okay, need password.";
        case ReplyCode::NEED_ACCOUNT_FOR_LOGIN: return "Need account for login.";
        case ReplyCode::FILE_ACTION_PENDING_INFO: return "Requested file action pending further information.";
        case ReplyCode::SERVICE_NOT_AVAILABLE: return "Service not available, closing control connection.";
        case ReplyCode::CANNOT_OPEN_DATA_CONN: return "Can't open data connection.";
        case ReplyCode::CONN_CLOSED_TRANSFER_ABORT: return "Connection closed; transfer aborted.";
        case ReplyCode::FILE_ACTION_NOT_TAKEN_TEMP: return "Requested file action not taken.";
        case ReplyCode::ACTION_ABORTED_LOCAL_ERR: return "Requested action aborted: local error in processing.";
        case ReplyCode::ACTION_NOT_TAKEN_NO_SPACE: return "Requested action not taken: insufficient storage space.";
        case ReplyCode::SYNTAX_ERROR_COMMAND: return "Syntax error, command unrecognized.";
        case ReplyCode::SYNTAX_ERROR_PARAMETERS: return "Syntax error in parameters or arguments.";
        case ReplyCode::COMMAND_NOT_IMPLEMENTED: return "Command not implemented.";
        case ReplyCode::BAD_SEQUENCE_OF_COMMANDS: return "Bad sequence of commands.";
        case ReplyCode::COMMAND_NOT_IMPLEMENTED_PAR: return "Command not implemented for that parameter.";
        case ReplyCode::NOT_LOGGED_IN: return "Not logged in.";
        case ReplyCode::NEED_ACCOUNT_FOR_STORING: return "Need account for storing files.";
        case ReplyCode::FILE_UNAVAILABLE: return "Requested action not taken: file unavailable or access denied.";
        case ReplyCode::PAGE_TYPE_UNKNOWN: return "Requested action aborted: page type unknown.";
        case ReplyCode::EXCEEDED_STORAGE_ALLOCATION: return "Requested file action aborted: exceeded storage allocation.";
        case ReplyCode::FILE_NAME_NOT_ALLOWED: return "Requested action not taken: file name not allowed.";
    }
    return "Action completed.";
}

// -----------------------------------------------------------------------------
// Self-contained, portable SHA-256 implementation (FIPS 180-4)
// -----------------------------------------------------------------------------
namespace {
    inline uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }
    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (~x & z);
    }
    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    inline uint32_t sig0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }
    inline uint32_t sig1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }
    inline uint32_t gam0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }
    inline uint32_t gam1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    struct Sha256Ctx {
        uint32_t state[8];
        uint64_t count;
        uint8_t buffer[64];
    };

    void sha256_init(Sha256Ctx& ctx) {
        ctx.state[0] = 0x6a09e667;
        ctx.state[1] = 0xbb67ae85;
        ctx.state[2] = 0x3c6ef372;
        ctx.state[3] = 0xa54ff53a;
        ctx.state[4] = 0x510e527f;
        ctx.state[5] = 0x9b05688c;
        ctx.state[6] = 0x1f83d9ab;
        ctx.state[7] = 0x5be0cd19;
        ctx.count = 0;
    }

    void sha256_transform(Sha256Ctx& ctx, const uint8_t data[64]) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
                   (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(data[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = gam1(w[i - 2]) + w[i - 7] + gam0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = ctx.state[0];
        uint32_t b = ctx.state[1];
        uint32_t c = ctx.state[2];
        uint32_t d = ctx.state[3];
        uint32_t e = ctx.state[4];
        uint32_t f = ctx.state[5];
        uint32_t g = ctx.state[6];
        uint32_t h = ctx.state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        ctx.state[0] += a;
        ctx.state[1] += b;
        ctx.state[2] += c;
        ctx.state[3] += d;
        ctx.state[4] += e;
        ctx.state[5] += f;
        ctx.state[6] += g;
        ctx.state[7] += h;
    }

    void sha256_update(Sha256Ctx& ctx, const uint8_t* data, size_t len) {
        size_t index = (ctx.count >> 3) & 63;
        ctx.count += (static_cast<uint64_t>(len) << 3);
        size_t partLen = 64 - index;
        size_t i = 0;

        if (len >= partLen) {
            std::memcpy(&ctx.buffer[index], data, partLen);
            sha256_transform(ctx, ctx.buffer);
            for (i = partLen; i + 63 < len; i += 64) {
                sha256_transform(ctx, &data[i]);
            }
            index = 0;
        }
        std::memcpy(&ctx.buffer[index], &data[i], len - i);
    }

    void sha256_final(Sha256Ctx& ctx, uint8_t digest[32]) {
        uint8_t bits[8];
        for (int i = 0; i < 8; ++i) {
            bits[i] = static_cast<uint8_t>((ctx.count >> ((7 - i) * 8)) & 0xFF);
        }
        size_t index = (ctx.count >> 3) & 63;
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);
        static const uint8_t PADDING[64] = { 0x80 };
        sha256_update(ctx, PADDING, padLen);
        sha256_update(ctx, bits, 8);

        for (int i = 0; i < 8; ++i) {
            digest[i * 4]     = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
        }
    }
}

std::string compute_data_sha256(const void* data, size_t len) {
    Sha256Ctx ctx;
    sha256_init(ctx);
    sha256_update(ctx, static_cast<const uint8_t*>(data), len);
    uint8_t digest[32];
    sha256_final(ctx, digest);

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string compute_file_sha256(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) return "";

    Sha256Ctx ctx;
    sha256_init(ctx);
    char buf[65536];
    while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
        sha256_update(ctx, reinterpret_cast<const uint8_t*>(buf), ifs.gcount());
    }
    uint8_t digest[32];
    sha256_final(ctx, digest);

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return oss.str();
}

} // namespace ftp
