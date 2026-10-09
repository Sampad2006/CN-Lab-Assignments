#ifndef A2_CRC_HPP
#define A2_CRC_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

namespace net {

// 16-bit internet checksum
uint16_t compute_checksum(const uint8_t* data, size_t len);
bool verify_checksum(const uint8_t* data, size_t len, uint16_t expected_checksum);

// cyclic redundancy checks
uint8_t  calculate_crc8(const uint8_t* data, size_t len);
uint16_t calculate_crc10(const uint8_t* data, size_t len);
uint16_t calculate_crc16(const uint8_t* data, size_t len);
uint32_t calculate_crc32(const uint8_t* data, size_t len);

// error detection methods
enum class ErrorDetectionMethod : int {
    CHECKSUM_16 = 0,
    CRC_8       = 1,
    CRC_10      = 2,
    CRC_16      = 3,
    CRC_32      = 4
};

uint32_t compute_fcs(ErrorDetectionMethod method, const uint8_t* data, size_t len);
bool verify_fcs(ErrorDetectionMethod method, const uint8_t* data, size_t len, uint32_t expected_fcs);

// error injection helpers
void inject_single_bit_error(uint8_t* data, size_t len);
void inject_double_bit_error(uint8_t* data, size_t len);
void inject_burst_error(uint8_t* data, size_t len, size_t burst_len, double flip_prob);
void inject_random_bit_errors(uint8_t* data, size_t len, size_t num_errors);

} // namespace net

#endif // A2_CRC_HPP
