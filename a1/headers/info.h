#ifndef INFO
#define INFO
#include <stdint.h>

// Frame info and error definitions
#define PAYLOAD_SIZE 44
//64 = 14+ 44 + 6
typedef struct frame_header{
    unsigned char SOURCE_MAC[6];
    unsigned char DEST_MAC[6];
    uint16_t length;
}frame_header;

typedef struct frame_trailer{
    uint32_t crc_send;
}frame_trailer;

typedef struct Frame{
    frame_header header;
    unsigned char payload[PAYLOAD_SIZE];
    frame_trailer trailer;
}frame;

void print_frame(frame* myframe);
void inject_random_bit_error(frame* my_frame,int num_errors);
void inject_burst_error(frame* my_frame,int burst_length,double flip_prob);
void inject_single_bit_error(frame* my_frame);
void inject_iso_double_bit_error(frame* my_frame);
void inject_odd_num_error(frame* my_frame);

#endif