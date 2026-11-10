#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define close_socket close

#include "../headers/info.h"
#include "../headers/error_injection.h"

#define PORT 9876
#define SERVER_IP "127.0.0.1"

int main(int argc,char* argv[]){
    srand(time(NULL));


    if(argc < 3){
        printf("Usage: %s <filename> <ip>\n",argv[0]);
        return EXIT_FAILURE;
    }

    const char* ip = argv[2];
    char* filename = argv[1];

    int detect_using;
    printf("Select Error Detection Method:\n");
    printf("0: Checksum\n");
    printf("1: CRC-8\n");
    printf("2: CRC-10\n");
    printf("3: CRC-16\n");
    printf("4: CRC-32\n");
    printf("Enter choice (0-4): ");
    if (scanf("%d", &detect_using) != 1 || detect_using < 0 || detect_using > 4) {
        printf("Invalid choice.\n");
        return EXIT_FAILURE;
    }

    int error_type;
    printf("Select Error Injection Type:\n");
    printf("0: None\n");
    printf("1: Single Bit Error\n");
    printf("2: Isolated Double Bit Error\n");
    printf("3: Odd Number of Errors\n");
    printf("4: Burst Error\n");
    printf("5: CRC-8 Collision Demo\n");
    printf("Enter choice (0-5): ");
    if (scanf("%d", &error_type) != 1 || error_type < 0 || error_type > 5) {
        printf("Invalid choice.\n");
        return EXIT_FAILURE;
    }

    int N = 0;
    if (error_type != 0) {
        printf("Enter the initial skip value (N) for periodic error injection: ");
        if (scanf("%d", &N) != 1 || N <= 0) {
            printf("Invalid value for N.\n");
            return EXIT_FAILURE;
        }
    }

    FILE* fp;
    fp = fopen(filename,"rb");
    if(fp == NULL){
        printf("Couldn't open file");
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET,SOCK_DGRAM,0);

    struct sockaddr_in recv_addr;

    memset(&recv_addr,0,sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(PORT);
    
    // Use the IP address provided in the command line argument
    inet_pton(AF_INET, ip, &recv_addr.sin_addr);

    size_t bytes_read;

    frame* myframe = malloc(sizeof(frame));

    int frame_idx = 0;
    int current_step = N;
    int target_frame = N;

    while((bytes_read = fread(myframe->payload,sizeof(char),PAYLOAD_SIZE,fp))){
        // Fill the rest of the array with zeros
        if (bytes_read < PAYLOAD_SIZE) {
            memset(&myframe->payload[bytes_read], 0, PAYLOAD_SIZE - bytes_read);
        }
        memset(myframe->header.DEST_MAC,0xAA,6);
        memset(myframe->header.SOURCE_MAC, 0xBB,6);
        myframe->header.length = bytes_read;

        // Calculate Checksum
        size_t checksum_len = sizeof(frame) - sizeof(frame_trailer);
        switch(detect_using){
            case 0: myframe->trailer.crc_send = compute_checksum((uint8_t*)myframe,checksum_len); break;
            case 1: myframe->trailer.crc_send = calculate_crc8((const uint8_t*)myframe,checksum_len); break;
            case 2: myframe->trailer.crc_send = calculate_crc10((const uint8_t*)myframe,checksum_len); break;
            case 3: myframe->trailer.crc_send = calculate_crc16((const uint8_t*)myframe,checksum_len); break;
            case 4: myframe->trailer.crc_send = calculate_crc32((const uint8_t*)myframe,checksum_len); break;
        }

        // ERROR INJECTION (Periodic / Counter-based)
        frame_idx++;
        int inject_error = 0;

        if (error_type != 0 && frame_idx == target_frame) {
            inject_error = 1;
            current_step--;
            if (current_step <= 0) {
                current_step = N;
            }
            target_frame += current_step;
            printf("[SENT] Frame %d corrupted. Next target is %d\n", frame_idx, target_frame);
        }

        if (inject_error) {
            switch (error_type)
            {
                case 0: break; // None
                case 1: inject_single_bit_error(myframe); break;
                case 2: inject_iso_double_bit_error(myframe); break;
                case 3: inject_odd_num_error(myframe); break;
                case 4: 
                {
                    size_t max_bits = checksum_len * 8;
                    size_t dyn_burst = 10 + (rand() % (max_bits - 10));
                    inject_burst_error(myframe, dyn_burst, 0.7); 
                    break;
                }
                case 5:
                {
                    // XOR Byte 0 with 0x01 and Byte 1 with 0xD5 of the payload
                    myframe->payload[0] ^= 0x01;
                    myframe->payload[1] ^= 0xD5;
                    break;
                }
            }
        }

        int sent_bytes = sendto(sockfd,(char*)myframe,sizeof(frame),0,
    (struct sockaddr*)&recv_addr,sizeof(recv_addr));
        
    }

    if (ferror(fp)) {
        printf("\nAn error occurred while reading the file.\n");
    } else if (feof(fp)) {
        printf("\nEOF\n");
    }

    close_socket(sockfd);

    fclose(fp);
    return EXIT_SUCCESS;

}
