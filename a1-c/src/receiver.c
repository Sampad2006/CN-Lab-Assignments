#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define close_socket close


#include "../headers/info.h"
#include "../headers/error_injection.h"

#define PORT 9876
#define SERVER_IP "127.0.0.1"

int main(int argc,char* argv[]){
    if(argc < 2){
        printf("Error: Missing argument.\nUsage: %s <out_filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* out_filename = argv[1];

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
    FILE* out_file = fopen(out_filename,"ab");
    if(out_file == NULL){
        return EXIT_FAILURE;
    }




    int sockfd = socket(AF_INET,SOCK_DGRAM,0);

    struct sockaddr_in recv_addr;
    struct sockaddr_in sender_addr;

    memset(&recv_addr,0,sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;//tcp
    recv_addr.sin_port = htons(PORT);//port
    
    // Instead of accepting the packets from a specific server_ip
    // inet_pton(AF_INET,SERVER_IP,&recv_addr.sin_addr);

    // receiver msgs from an ip as long as they hit the correct port
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    printf("Socket created successfully\n");

    bind(sockfd,(struct sockaddr*)&recv_addr,sizeof(recv_addr));
    printf("Socket bound to port %d\n",PORT);

    socklen_t addr_len = sizeof(sender_addr);
    frame* recdata  = malloc(sizeof(frame));

    while(1){
        // receive bytes and print out the bytes to console;
        int bytes_received = recvfrom(sockfd,(char*)recdata,sizeof(frame),0,(struct sockaddr*)&sender_addr,&addr_len);
        if (bytes_received < 0){printf("Error receiving packet!\n"); continue;}
        printf("received %d bytes\n",bytes_received);
        
        // The receiver runs the same checksum on the data
        size_t checksum_len = sizeof(frame) - sizeof(frame_trailer);
        
        uint32_t computed_val = 0;
        
        switch(detect_using){
            case 0: computed_val = compute_checksum((uint8_t*)recdata,checksum_len); break;
            case 1: computed_val = calculate_crc8((const uint8_t*)recdata,checksum_len);break;
            case 2: computed_val = calculate_crc10((const uint8_t*)recdata,checksum_len); break;
            case 3: computed_val = calculate_crc16((const uint8_t*)recdata,checksum_len); break;
            case 4: computed_val = calculate_crc32((const uint8_t*)recdata,checksum_len); break;
        }

        // Start the frame block in the file
        const char* start_tag = "\n<START>\n";
        fwrite(start_tag, 1, strlen(start_tag), out_file);

        if (computed_val == recdata->trailer.crc_send){
            printf("[ACCEPT] Frame received without error\n");
            const char* valid_flag = "STATUS: VALID\n";
            fwrite(valid_flag, 1, strlen(valid_flag), out_file);
        } else{
            printf("[Error Detected]\n");
            const char* corrupt_flag = "STATUS: CORRUPTED\n";
            fwrite(corrupt_flag, 1, strlen(corrupt_flag), out_file);
        }

        // Add additional info: payload length
        char length_info[50];
        snprintf(length_info, sizeof(length_info), "Length: %u bytes\n", recdata->header.length);
        fwrite(length_info, 1, strlen(length_info), out_file);

        // Write the payload (whether it's good or corrupted)
        fwrite(recdata->payload, 1, recdata->header.length, out_file);
        
        // End the frame block
        const char* end_tag = "\n<END>\n";
        fwrite(end_tag, 1, strlen(end_tag), out_file);
        
        fflush(out_file);
    }

    close_socket(sockfd);
    printf("Socket closed successfully\n");

    return EXIT_SUCCESS;
}
