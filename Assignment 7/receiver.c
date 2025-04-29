#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 2048

int main()
{
    // Create raw socket
    int sock = socket(AF_INET, SOCK_RAW, 253); // Custom protocol number
    if (sock < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    printf("Raw socket created successfully. Waiting for packets...\n");

    // Buffer to store received packets
    char buffer[BUFFER_SIZE];
    struct sockaddr_in source;
    socklen_t source_len = sizeof(source);

    while (1)
    {
        // Receive packet
        int received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                (struct sockaddr *)&source, &source_len);
        if (received < 0)
        {
            perror("recvfrom failed");
            close(sock);
            return 1;
        }

        // Extract IP header
        struct iphdr *ip_header = (struct iphdr *)buffer;

        // Calculate IP header length
        int ip_header_len = ip_header->ihl * 4;

        // Extract payload
        int payload_len = received - ip_header_len;
        char *payload = buffer + ip_header_len;

        // Ensure null-termination
        payload[payload_len] = '\0';

        // Print information
        printf("\n----- Packet Received -----\n");
        printf("Size: %d bytes\n", received);
        printf("Source IP: %s\n", inet_ntoa(source.sin_addr));
        printf("Protocol: %d\n", ip_header->protocol);
        printf("Message: %s\n", payload);
        printf("---------------------------\n");
    }

    close(sock);
    return 0;
}
