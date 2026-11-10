#include "../headers/error_injection.h"
//header
uint16_t compute_checksum(uint8_t* data, size_t len) {
    uint32_t sum = 0;
    // adjacent 8-bit bytes into 16-bit words-shift
    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = (data[i] << 8) | ((i + 1 < len) ? data[i + 1] : 0);
        sum += word;

        //wrap-around
        if (sum > 0xFFFF) {
            sum= (sum & 0xFFFF) + 1;
        }
    }

    //complement of sum
    return (uint16_t)(~sum);
}
bool verify_checksum(uint8_t* frame, size_t frame_len) {
    // complement sum over all fr(including checksum)
    uint32_t sum = 0;

    for (size_t i = 0; i < frame_len; i += 2) {
        uint16_t word = (frame[i]<<8)|((i+1<frame_len)?frame[i + 1]:0);
        sum += word;

        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    //inversion of sum->0 syndrome check
    uint16_t fin=(uint16_t)(~sum);
    return (fin==0x0000);
}

uint64_t calculate_crc(const uint8_t* data,size_t length,uint64_t polynomial,unsigned width)
{
    uint64_t crc = 0;
    uint64_t top_bit = 1ULL << (width - 1);//msb
    uint64_t mask = (1ULL << width) - 1; //mask

    for (size_t i = 0; i < length; i++) {

        crc ^= (uint64_t)data[i] << (width - 8);
        for (size_t j = 0; j < 8; j++) {//for every bit
            if (crc & top_bit)
                crc = (crc << 1) ^ polynomial;
            else
                crc <<= 1;

            crc &= mask;
        }
    }

    return crc;
}

uint8_t calculate_crc8(const uint8_t* data,size_t length){
    return (uint8_t)calculate_crc(data,length,0xD5,8);
}

uint16_t calculate_crc10(const uint8_t* data,size_t length){
    return (uint16_t)calculate_crc(data,length,0x233,10);
}

uint16_t calculate_crc16(const uint8_t* data,size_t length){
    return (uint16_t)calculate_crc(data,length,0x8005,16);
}

uint32_t calculate_crc32(const uint8_t* data, size_t length)
{
    return (uint32_t)calculate_crc(data,length,0x04C11DB7,32);
}
