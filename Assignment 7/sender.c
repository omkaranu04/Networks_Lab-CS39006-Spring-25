#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/sysinfo.h>

#define PACKET_SIZE 1024

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 3)
    {
        printf("Usage: %s <destination_ip> <message>\n", argv[0]);
        return 1;
    }

    // Create raw socket
    int sock = socket(AF_INET, SOCK_RAW, 253); // Custom protocol number
    if (sock < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Tell the kernel we'll provide the IP header
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("setsockopt failed");
        close(sock);
        return 1;
    }

    // Prepare packet buffer
    char packet[PACKET_SIZE];
    memset(packet, 0, PACKET_SIZE);

    // IP header pointer
    struct iphdr *ip = (struct iphdr *)packet;

    // Set up IP header
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = sizeof(struct iphdr) + strlen(argv[2]);
    ip->id = htons(12345);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = 253;                 // Custom protocol number
    ip->check = 0;                      // Set to 0 before calculating checksum
    ip->saddr = inet_addr("127.0.0.1"); // Source IP (localhost)
    ip->daddr = inet_addr(argv[1]);     // Destination IP

    // Copy the message to the packet buffer after the IP header
    strcpy(packet + sizeof(struct iphdr), argv[2]);

    // Set up destination address
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = ip->daddr;

    // Send the packet
    int sent = sendto(sock, packet, ip->tot_len, 0,
                      (struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0)
    {
        perror("sendto failed");
    }
    else
    {
        printf("Successfully sent %d bytes!\n", sent);
        printf("Message: %s\n", argv[2]);
    }

    close(sock);
    return 0;
}
