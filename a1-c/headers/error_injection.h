#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// computes 16-bit 1's complement checksum over input buffer
uint16_t compute_checksum(uint8_t* data,size_t len);

// validates a block containing data + 16-bit checksum trailer= 0x0000
bool verify_checksum(uint8_t* frame,size_t frame_len);

// CRC functions
uint64_t calculate_crc(const uint8_t* data, size_t length, uint64_t polynomial, unsigned width);
uint8_t calculate_crc8(const uint8_t* data, size_t length);
uint16_t calculate_crc10(const uint8_t* data, size_t length);
uint16_t calculate_crc16(const uint8_t* data, size_t length);
uint32_t calculate_crc32(const uint8_t* data, size_t length);

#endif // CHECKSUM_H