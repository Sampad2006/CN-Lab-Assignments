#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../headers/info.h"
#include "../headers/error_injection.h"

#define TRIALS 200000
#define TIMING_ITERATIONS 5000000

// Helper to fill a frame with random data
void randomize_frame(frame* f) {
    for (size_t i = 0; i < 6; i++) {
        f->header.SOURCE_MAC[i] = rand() % 256;
        f->header.DEST_MAC[i] = rand() % 256;
    }
    f->header.length = PAYLOAD_SIZE;
    for (size_t i = 0; i < PAYLOAD_SIZE; i++) {
        f->payload[i] = rand() % 256;
    }
    f->trailer.crc_send = 0;
}

// Struct to store overlap results
typedef struct {
    long both_detected;
    long checksum_only;
    long crc_only;
    long neither;
} OverlapResult;

// Print a frame in hex for debugging/examples
void hex_dump(const char* label, const uint8_t* data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// Finds and prints concrete examples of overlap cases
void find_concrete_examples() {
    printf("\n### Concrete Examples of Error Detection Cases (with CRC-8)\n\n");
    
    frame orig, corrupted;
    size_t len = sizeof(frame) - sizeof(frame_trailer);
    
    bool found_both = false;
    bool found_checksum_only = false;
    bool found_crc_only = false;
    
    // Seed with a fixed value for deterministic example search
    srand(42);
    
    for (int i = 0; i < 1000000; i++) {
        randomize_frame(&orig);
        
        uint16_t orig_cksum = compute_checksum((uint8_t*)&orig, len);
        uint8_t orig_crc8 = calculate_crc8((uint8_t*)&orig, len);
        
        memcpy(&corrupted, &orig, sizeof(frame));
        
        // Inject a random burst or multi-bit error to search for cases
        if (i % 3 == 0) {
            inject_random_bit_error(&corrupted, 2 + (rand() % 4));
        } else {
            inject_burst_error(&corrupted, 8 + (rand() % 16), 0.8);
        }
        
        uint16_t corr_cksum = compute_checksum((uint8_t*)&corrupted, len);
        uint8_t corr_crc8 = calculate_crc8((uint8_t*)&corrupted, len);
        
        bool cksum_detected = (orig_cksum != corr_cksum);
        bool crc8_detected = (orig_crc8 != corr_crc8);
        
        if (cksum_detected && crc8_detected && !found_both) {
            printf("**Case 1: Error detected by both CRC-8 and Checksum**\n");
            printf("- Original Checksum: 0x%04X, Corrupted Checksum: 0x%04X (Detected: Yes)\n", orig_cksum, corr_cksum);
            printf("- Original CRC-8: 0x%02X, Corrupted CRC-8: 0x%02X (Detected: Yes)\n", orig_crc8, corr_crc8);
            hex_dump("  Original Frame (first 20 bytes)", (uint8_t*)&orig, 20);
            hex_dump("  Corrupted Frame (first 20 bytes)", (uint8_t*)&corrupted, 20);
            printf("\n");
            found_both = true;
        }
        
        if (cksum_detected && !crc8_detected && !found_checksum_only) {
            printf("**Case 2: Error detected by Checksum but NOT by CRC-8**\n");
            printf("- Original Checksum: 0x%04X, Corrupted Checksum: 0x%04X (Detected: Yes)\n", orig_cksum, corr_cksum);
            printf("- Original CRC-8: 0x%02X, Corrupted CRC-8: 0x%02X (Detected: No - Collision!)\n", orig_crc8, corr_crc8);
            hex_dump("  Original Frame (first 20 bytes)", (uint8_t*)&orig, 20);
            hex_dump("  Corrupted Frame (first 20 bytes)", (uint8_t*)&corrupted, 20);
            printf("\n");
            found_checksum_only = true;
        }
        
        if (!cksum_detected && crc8_detected && !found_crc_only) {
            printf("**Case 3: Error detected by CRC-8 but NOT by Checksum**\n");
            printf("- Original Checksum: 0x%04X, Corrupted Checksum: 0x%04X (Detected: No - Collision!)\n", orig_cksum, corr_cksum);
            printf("- Original CRC-8: 0x%02X, Corrupted CRC-8: 0x%02X (Detected: Yes)\n", orig_crc8, corr_crc8);
            hex_dump("  Original Frame (first 20 bytes)", (uint8_t*)&orig, 20);
            hex_dump("  Corrupted Frame (first 20 bytes)", (uint8_t*)&corrupted, 20);
            printf("\n");
            found_crc_only = true;
        }
        
        if (found_both && found_checksum_only && found_crc_only) {
            break;
        }
    }
    
    if (!found_checksum_only) {
        printf("*(Note: A case where Checksum detects but CRC-8 misses was not found in 1M random trials due to the low probability of CRC-8 collisions on small multi-bit errors. Checksum is generally weaker than CRC-8).* \n\n");
    }
}

// Runs verification simulations
void run_detection_simulation(const char* error_name, int error_type) {
    printf("#### Error Model: %s\n\n", error_name);
    printf("| Scheme | Both Detected | Checksum Only | CRC Only | Neither (Missed Both) | CRC Miss Rate | Checksum Miss Rate |\n");
    printf("|--------|---------------|---------------|----------|----------------------|---------------|--------------------|\n");
    
    frame orig, corrupted;
    size_t len = sizeof(frame) - sizeof(frame_trailer);
    
    // counts for CRC-8, CRC-10, CRC-16, CRC-32
    OverlapResult results[4] = {0};
    long total_checksum_detected = 0;
    
    for (int t = 0; t < TRIALS; t++) {
        randomize_frame(&orig);
        
        uint16_t orig_cksum = compute_checksum((uint8_t*)&orig, len);
        uint8_t orig_crc8 = calculate_crc8((uint8_t*)&orig, len);
        uint16_t orig_crc10 = calculate_crc10((uint8_t*)&orig, len);
        uint16_t orig_crc16 = calculate_crc16((uint8_t*)&orig, len);
        uint32_t orig_crc32 = calculate_crc32((uint8_t*)&orig, len);
        
        memcpy(&corrupted, &orig, sizeof(frame));
        
        switch (error_type) {
            case 1: inject_single_bit_error(&corrupted); break;
            case 2: inject_iso_double_bit_error(&corrupted); break;
            case 3: inject_odd_num_error(&corrupted); break;
            case 4: inject_burst_error(&corrupted, 12, 0.7); break; // 12-bit burst
            case 5: inject_burst_error(&corrupted, 32, 0.7); break; // 32-bit burst
        }
        
        uint16_t corr_cksum = compute_checksum((uint8_t*)&corrupted, len);
        uint8_t corr_crc8 = calculate_crc8((uint8_t*)&corrupted, len);
        uint16_t corr_crc10 = calculate_crc10((uint8_t*)&corrupted, len);
        uint16_t corr_crc16 = calculate_crc16((uint8_t*)&corrupted, len);
        uint32_t corr_crc32 = calculate_crc32((uint8_t*)&corrupted, len);
        
        bool cksum_det = (orig_cksum != corr_cksum);
        if (cksum_det) total_checksum_detected++;
        
        bool crc_det[4] = {
            (orig_crc8 != corr_crc8),
            (orig_crc10 != corr_crc10),
            (orig_crc16 != corr_crc16),
            (orig_crc32 != corr_crc32)
        };
        
        for (int i = 0; i < 4; i++) {
            if (cksum_det && crc_det[i]) results[i].both_detected++;
            else if (cksum_det && !crc_det[i]) results[i].checksum_only++;
            else if (!cksum_det && crc_det[i]) results[i].crc_only++;
            else results[i].neither++;
        }
    }
    
    const char* names[4] = {"CRC-8", "CRC-10", "CRC-16", "CRC-32"};
    for (int i = 0; i < 4; i++) {
        double both_p = (double)results[i].both_detected / TRIALS * 100.0;
        double ck_p = (double)results[i].checksum_only / TRIALS * 100.0;
        double crc_p = (double)results[i].crc_only / TRIALS * 100.0;
        double none_p = (double)results[i].neither / TRIALS * 100.0;
        
        double crc_miss_p = (ck_p + none_p);
        double cksum_miss_p = (crc_p + none_p);
        
        printf("| %s | %.4f%% | %.4f%% | %.4f%% | %.4f%% | %.4f%% | %.4f%% |\n",
               names[i], both_p, ck_p, crc_p, none_p, crc_miss_p, cksum_miss_p);
    }
    printf("\n");
}

// Runs speed/performance benchmark
void run_speed_benchmark() {
    printf("### Computational Efficiency Benchmarks (%d iterations)\n\n", TIMING_ITERATIONS);
    printf("| Scheme | Total Time (s) | Avg Time per Frame (ns) | Throughput (MB/s) |\n");
    printf("|--------|----------------|-------------------------|-------------------|\n");
    
    frame f;
    randomize_frame(&f);
    size_t len = sizeof(frame) - sizeof(frame_trailer);
    double total_bytes = (double)len * TIMING_ITERATIONS;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    
    struct timespec start, end;
    double elapsed;
    
    // 1. Checksum
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        volatile uint16_t res = compute_checksum((uint8_t*)&f, len);
        (void)res;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("| Checksum | %.4f s | %.2f ns | %.2f MB/s |\n", 
           elapsed, (elapsed / TIMING_ITERATIONS) * 1e9, total_mb / elapsed);
           
    // 2. CRC-8
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        volatile uint8_t res = calculate_crc8((uint8_t*)&f, len);
        (void)res;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("| CRC-8 | %.4f s | %.2f ns | %.2f MB/s |\n", 
           elapsed, (elapsed / TIMING_ITERATIONS) * 1e9, total_mb / elapsed);

    // 3. CRC-10
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        volatile uint16_t res = calculate_crc10((uint8_t*)&f, len);
        (void)res;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("| CRC-10 | %.4f s | %.2f ns | %.2f MB/s |\n", 
           elapsed, (elapsed / TIMING_ITERATIONS) * 1e9, total_mb / elapsed);

    // 4. CRC-16
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        volatile uint16_t res = calculate_crc16((uint8_t*)&f, len);
        (void)res;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("| CRC-16 | %.4f s | %.2f ns | %.2f MB/s |\n", 
           elapsed, (elapsed / TIMING_ITERATIONS) * 1e9, total_mb / elapsed);

    // 5. CRC-32
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TIMING_ITERATIONS; i++) {
        volatile uint32_t res = calculate_crc32((uint8_t*)&f, len);
        (void)res;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("| CRC-32 | %.4f s | %.2f ns | %.2f MB/s |\n", 
           elapsed, (elapsed / TIMING_ITERATIONS) * 1e9, total_mb / elapsed);
    
    printf("\n");
}

int main() {
    srand(time(NULL));
    
    printf("# Comparative Evaluation Results (Checksum vs CRC)\n\n");
    printf("This report provides a comparative study of the error detection rates, overlap, and computational efficiency of the 16-bit Checksum against various CRC options (CRC-8, CRC-10, CRC-16, and CRC-32).\n\n");
    
    printf("## 1. Error Detection & Overlap Analysis (%d trials per model)\n\n", TRIALS);
    run_detection_simulation("Single Bit Error", 1);
    run_detection_simulation("Isolated Double Bit Error", 2);
    run_detection_simulation("Odd Number of Errors", 3);
    run_detection_simulation("Burst Error (Length 12)", 4);
    run_detection_simulation("Burst Error (Length 32)", 5);
    
    printf("## 2. Speed and Performance Benchmarks\n\n");
    run_speed_benchmark();
    
    printf("## 3. Boundary Cases and Concrete Examples\n\n");
    find_concrete_examples();
    
    return 0;
}
