#include <stdio.h>
#include <stdlib.h>

#include "../headers/info.h"
#include "../headers/error_injection.h"

void print_frame(frame* myframe){
    printf("Message in packet: %s",myframe->payload);
    printf("=====");
}

void inject_random_bit_error(frame* my_frame,int num_errors){
    size_t frame_size = sizeof(frame) - sizeof(frame_trailer);
    if(num_errors > frame_size * 8){
        printf("Error num exceeded frame_size. Please try a smaller number");
        return;
    }
    unsigned char* byte_ptr =  (unsigned char*)my_frame;
    
    // Array to keep track of flipped bits to ensure distinct bits
    int flipped[512] = {0}; // frame_size * 8 is <= 464

    for(size_t i = 0; i < num_errors; i++){
        size_t rand_byte;
        size_t rand_bit_idx;
        int flat_idx;
        do {
            rand_byte = rand() % frame_size;
            rand_bit_idx = rand() % 8;
            flat_idx = rand_byte * 8 + rand_bit_idx;
        } while (flipped[flat_idx] == 1);
        
        flipped[flat_idx] = 1;
        byte_ptr[rand_byte] = byte_ptr[rand_byte] ^ (1 << rand_bit_idx); 
    }
}

void inject_burst_error(frame* my_frame,int burst_length,double flip_prob){
    size_t total_bits = (sizeof(frame) - sizeof(frame_trailer)) * 8;
    // burst length is in bits
    if(burst_length > total_bits){
        printf("Error num exceeded frame_size. Please try a smaller burst length");
        return;
    }
    unsigned char* byte_ptr = (unsigned char*)my_frame;
    size_t start_bit = rand() % (total_bits - burst_length + 1);

    for(size_t i = 0; i < burst_length; i++){
        double r = (double)rand() / RAND_MAX;
        if (r < flip_prob){
            size_t curr_bit = start_bit + i;
            size_t byte_idx = curr_bit / 8;
            size_t bit_idx = curr_bit % 8;
            byte_ptr[byte_idx] = byte_ptr[byte_idx] ^ (1 << bit_idx);
        }
    }
}

void inject_single_bit_error(frame* my_frame){
    inject_random_bit_error(my_frame,1);
}

void inject_iso_double_bit_error(frame* my_frame){
    inject_random_bit_error(my_frame,2);
}

void inject_odd_num_error(frame* my_frame){
    //1, 3, 5, or 7
    size_t rand_odd_num = (rand() % 4) * 2 + 1;
    inject_random_bit_error(my_frame, rand_odd_num);
}