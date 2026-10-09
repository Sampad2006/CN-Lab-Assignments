#include "crc.hpp"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <random>

using namespace std;

namespace net {

// 16-bit internet checksum
uint16_t compute_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = (static_cast<uint16_t>(data[i]) << 8) |
                        ((i + 1 < len) ? static_cast<uint16_t>(data[i + 1]) : 0);
        sum += word;
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + 1;
        }
    }
    return static_cast<uint16_t>(~sum);
}

bool verify_checksum(const uint8_t* data, size_t len, uint16_t expected_checksum) {
    return (compute_checksum(data, len) == expected_checksum);
}

// bitwise crc division
static uint64_t calculate_crc_generic(const uint8_t* data, size_t length, uint64_t polynomial, unsigned width) {
    uint64_t crc = 0;
    uint64_t top_bit = 1ULL << (width - 1);
    uint64_t mask = (1ULL << width) - 1;

    for (size_t i = 0; i < length; i++) {
        crc ^= (static_cast<uint64_t>(data[i]) << (width - 8));
        for (size_t j = 0; j < 8; j++) {
            if (crc & top_bit) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
            crc &= mask;
        }
    }
    return crc;
}

uint8_t calculate_crc8(const uint8_t* data, size_t len) {
    return static_cast<uint8_t>(calculate_crc_generic(data, len, 0xD5, 8));
}

uint16_t calculate_crc10(const uint8_t* data, size_t len) {
    return static_cast<uint16_t>(calculate_crc_generic(data, len, 0x233, 10));
}

uint16_t calculate_crc16(const uint8_t* data, size_t len) {
    return static_cast<uint16_t>(calculate_crc_generic(data, len, 0x8005, 16));
}

uint32_t calculate_crc32(const uint8_t* data, size_t len) {
    return static_cast<uint32_t>(calculate_crc_generic(data, len, 0x04C11DB7, 32));
}

uint32_t compute_fcs(ErrorDetectionMethod method, const uint8_t* data, size_t len) {
    switch (method) {
        case ErrorDetectionMethod::CHECKSUM_16:
            return compute_checksum(data, len);
        case ErrorDetectionMethod::CRC_8:
            return calculate_crc8(data, len);
        case ErrorDetectionMethod::CRC_10:
            return calculate_crc10(data, len);
        case ErrorDetectionMethod::CRC_16:
            return calculate_crc16(data, len);
        case ErrorDetectionMethod::CRC_32:
        default:
            return calculate_crc32(data, len);
    }
}

bool verify_fcs(ErrorDetectionMethod method, const uint8_t* data, size_t len, uint32_t expected_fcs) {
    return (compute_fcs(method, data, len) == expected_fcs);
}

// error injection helpers
void inject_single_bit_error(uint8_t* data, size_t len) {
    if (len == 0) return;
    size_t byte_idx = rand() % len;
    size_t bit_idx = rand() % 8;
    data[byte_idx] ^= (1 << bit_idx);
}

void inject_double_bit_error(uint8_t* data, size_t len) {
    if (len == 0) return;
    inject_single_bit_error(data, len);
    inject_single_bit_error(data, len);
}

void inject_random_bit_errors(uint8_t* data, size_t len, size_t num_errors) {
    if (len == 0 || num_errors == 0) return;
    size_t total_bits = len * 8;
    if (num_errors > total_bits) num_errors = total_bits;

    vector<bool> flipped(total_bits, false);
    for (size_t i = 0; i < num_errors; i++) {
        size_t bit_pos;
        do {
            bit_pos = rand() % total_bits;
        } while (flipped[bit_pos]);
        flipped[bit_pos] = true;
        data[bit_pos / 8] ^= (1 << (bit_pos % 8));
    }
}

void inject_burst_error(uint8_t* data, size_t len, size_t burst_len, double flip_prob) {
    if (len == 0 || burst_len == 0) return;
    size_t total_bits = len * 8;
    if (burst_len > total_bits) burst_len = total_bits;

    size_t start_bit = rand() % (total_bits - burst_len + 1);
    for (size_t i = 0; i < burst_len; i++) {
        double r = static_cast<double>(rand()) / RAND_MAX;
        if (r < flip_prob || i == 0 || i == burst_len - 1) {
            size_t curr_bit = start_bit + i;
            data[curr_bit / 8] ^= (1 << (curr_bit % 8));
        }
    }
}

} // namespace net
