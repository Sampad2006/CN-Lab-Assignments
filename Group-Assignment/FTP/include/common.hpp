#ifndef FTP_COMMON_HPP
#define FTP_COMMON_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace ftp {

// Default networking constants
constexpr uint16_t DEFAULT_FTP_PORT = 2121;
constexpr size_t BUFFER_SIZE = 65536; // 64 KB chunk size for high-throughput stream
constexpr int CONTROL_TIMEOUT_SEC = 300; // 5 minutes idle timeout for interactive commands
constexpr int DATA_TIMEOUT_SEC = 60;     // 60 seconds timeout for data transfers

/**
 * Standard RFC 959 & RFC 3659 FTP Status Reply Codes
 */
enum class ReplyCode : int {
    // 1xx: Positive Preliminary Reply
    RESTART_MARKER              = 110,
    SERVICE_READY_MINUTES       = 120,
    DATA_CONN_ALREADY_OPEN      = 125,
    FILE_STATUS_OK_OPEN_DATA    = 150,

    // 2xx: Positive Completion Reply
    COMMAND_OK                  = 200,
    COMMAND_NOT_IMPLEMENTED_SUP = 202,
    SYSTEM_STATUS               = 211,
    DIRECTORY_STATUS            = 212,
    FILE_STATUS                 = 213,
    HELP_MESSAGE                = 214,
    SYSTEM_NAME_TYPE            = 215, // e.g. UNIX Type: L8
    SERVICE_READY               = 220,
    CLOSING_CONTROL_CONN        = 221,
    DATA_CONN_OPEN_NO_TRANSFER  = 225,
    CLOSING_DATA_CONN_SUCCESS   = 226,
    ENTERING_PASSIVE_MODE       = 227,
    ENTERING_EXTENDED_PASSIVE   = 229,
    USER_LOGGED_IN              = 230,
    REQUESTED_FILE_ACTION_OK    = 250,
    PATHNAME_CREATED            = 257,

    // 3xx: Positive Intermediate Reply
    USER_NAME_OK_NEED_PASS      = 331,
    NEED_ACCOUNT_FOR_LOGIN      = 332,
    FILE_ACTION_PENDING_INFO    = 350,

    // 4xx: Transient Negative Completion Reply
    SERVICE_NOT_AVAILABLE       = 421,
    CANNOT_OPEN_DATA_CONN       = 425,
    CONN_CLOSED_TRANSFER_ABORT  = 426,
    FILE_ACTION_NOT_TAKEN_TEMP  = 450,
    ACTION_ABORTED_LOCAL_ERR    = 451,
    ACTION_NOT_TAKEN_NO_SPACE   = 452,

    // 5xx: Permanent Negative Completion Reply
    SYNTAX_ERROR_COMMAND        = 500,
    SYNTAX_ERROR_PARAMETERS     = 501,
    COMMAND_NOT_IMPLEMENTED     = 502,
    BAD_SEQUENCE_OF_COMMANDS    = 503,
    COMMAND_NOT_IMPLEMENTED_PAR = 504,
    NOT_LOGGED_IN               = 530,
    NEED_ACCOUNT_FOR_STORING    = 532,
    FILE_UNAVAILABLE            = 550,
    PAGE_TYPE_UNKNOWN           = 551,
    EXCEEDED_STORAGE_ALLOCATION = 552,
    FILE_NAME_NOT_ALLOWED       = 553
};

/**
 * RFC 959 Data Representation Types
 */
enum class TransferType {
    ASCII,        // TYPE A: ASCII text format (CRLF line endings)
    BINARY_IMAGE  // TYPE I: Image/Binary format (octet-exact stream)
};

/**
 * Data connection transport mode
 */
enum class DataMode {
    NONE,
    ACTIVE,          // PORT h1,h2,h3,h4,p1,p2 (Server connects to Client)
    PASSIVE,         // PASV -> Server listens, Client connects
    EXTENDED_PASSIVE // EPSV (RFC 2428)
};

/**
 * Client authentication state machine
 */
enum class AuthState {
    UNAUTHENTICATED,
    AWAITING_PASSWORD,
    AUTHENTICATED
};

/**
 * IP and Port Tuple for PORT / PASV / EPRT / EPSV translations
 */
struct IpPortTuple {
    std::string ip;
    uint16_t port{0};

    IpPortTuple() = default;
    IpPortTuple(std::string ip_addr, uint16_t p) : ip(std::move(ip_addr)), port(p) {}

    // Formats into RFC 959 comma-separated format: h1,h2,h3,h4,p1,p2
    std::string to_comma_format() const;

    // Formats into standard address string: 127.0.0.1:2121
    std::string to_string() const;

    // Parses RFC 959 "h1,h2,h3,h4,p1,p2"
    static bool from_comma_format(const std::string& str, IpPortTuple& out);

    // Parses "(h1,h2,h3,h4,p1,p2)" from a 227 PASV reply
    static bool from_pasv_reply(const std::string& reply_text, IpPortTuple& out);

    // Parses RFC 2428 Extended Passive reply e.g. "Entering Extended Passive Mode (|||port|)"
    static bool from_epsv_reply(const std::string& reply_text, uint16_t& out_port);
};

// String utility functions
std::string trim(const std::string& s);
std::string to_upper(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::string format_bytes(uint64_t bytes);
std::string get_default_reply_text(ReplyCode code);

// Fast file SHA-256 / Checksum for integrity verification
std::string compute_file_sha256(const std::string& filepath);
std::string compute_data_sha256(const void* data, size_t len);

} // namespace ftp

#endif // FTP_COMMON_HPP
