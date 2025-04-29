/*
    Assignment 7 : CLDP Server
    Name: Omkar Vijay Bhandare
    Roll number: 22CS30016
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>

#define CLDP 253
#define HELLO 1
#define QUERY 2
#define RESPONSE 3
#define MAX_BUFFER 2048
#define INTERVAL 5

// header format custom
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t payload;
    uint16_t tid;
    uint32_t evenbit; // redundant
} CLDPHeader;

int sockfd;
struct sockaddr_in server_addr;

uint16_t checksum(uint16_t *header, int no_words)
{
    uint32_t sum = 0;
    for (int i = 0; i < no_words; i++)
        sum += header[i];
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

in_addr_t get_local_ip()
{
    struct ifaddrs *ifaddr, *ifa;
    in_addr_t ip = htonl(INADDR_LOOPBACK);

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs failed");
        return ip;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, "lo") != 0)
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            ip = addr->sin_addr.s_addr;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ip;
}

void *hello_thread_handler(void *args)
{
    char packet_buffer[MAX_BUFFER];

    while (1)
    {
        memset(packet_buffer, 0, MAX_BUFFER);

        struct iphdr *ip = (struct iphdr *)packet_buffer;
        ip->ihl = 5;
        ip->version = 4;
        ip->tos = 0;
        ip->tot_len = htons(sizeof(struct iphdr) + sizeof(CLDPHeader));
        ip->id = htons(rand() % 65535);
        ip->frag_off = 0;
        ip->ttl = 64;
        ip->protocol = CLDP;
        ip->check = 0;
        ip->saddr = get_local_ip();
        ip->daddr = inet_addr("255.255.255.255"); // broadcast the packet

        CLDPHeader *cldp = (CLDPHeader *)(packet_buffer + sizeof(struct iphdr));
        cldp->type = HELLO;
        cldp->payload = 0;
        cldp->tid = htons(0);
        cldp->evenbit = 0;

        ip->check = checksum((uint16_t *)ip, ip->ihl * 2);

        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = ip->daddr;

        if (sendto(sockfd, packet_buffer, ntohs(ip->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
            perror("failed to send HELLO\n");
        else
            printf("HELLO broadcasted...\n");

        sleep(INTERVAL);
    }
    return NULL;
}
int main(int argc, char const *argv[])
{
    printf("CLDP Server Starting..\n");
    sockfd = socket(AF_INET, SOCK_RAW, CLDP);
    if (sockfd < 0)
    {
        perror("Error creating socket\n");
        exit(1);
    }
    // set socket options, enable broadcast permission
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("Error setting IP_HDRINCL\n");
        close(sockfd);
        exit(1);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0)
    {
        perror("Error setting SO_BROADCAST\n");
        close(sockfd);
        exit(1);
    }

    // get local IP and hostname
    char hostname[512];
    if (gethostname(hostname, sizeof(hostname)) < 0)
    {
        perror("Failed to gethostname\n");
        close(sockfd);
        exit(1);
    }
    struct hostent *host_info = gethostbyname(hostname);
    if (host_info == NULL)
    {
        perror("Failed to gethostbyname\n");
        close(sockfd);
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);

    pthread_t hello_thread;
    if (pthread_create(&hello_thread, NULL, hello_thread_handler, NULL) != 0)
    {
        perror("Failed ot create HELLO thread\n");
        close(sockfd);
        exit(1);
    }

    char buffer[MAX_BUFFER];
    printf("Server listening...\n");

    while (1)
    {
        memset(buffer, 0, MAX_BUFFER);

        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        ssize_t n = recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &len);
        if (n < 0)
        {
            perror("error in recvfrom");
            continue;
        }

        struct iphdr *header = (struct iphdr *)buffer;
        if (header->protocol != CLDP)
            continue; // not the required packet

        int header_len = header->ihl * 4;
        if (n < header_len + sizeof(CLDPHeader))
            continue; // glitch in packet

        CLDPHeader *pack_header = (CLDPHeader *)(buffer + header_len);

        if (pack_header->type == QUERY) // if QUERY type packet
        {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(header->saddr), client_ip, INET_ADDRSTRLEN);
            printf("Received QUERY from %s (tid : %u)\n", client_ip, ntohs(pack_header->tid));

            char host[512]; // get system information
            gethostname(host, sizeof(host));

            struct timeval tv;
            gettimeofday(&tv, NULL);
            struct tm *time_info = localtime(&tv.tv_sec);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

            struct sysinfo sys_info;
            sysinfo(&sys_info);
            double load = (double)sys_info.loads[0] / (1 << SI_LOAD_SHIFT);

            // create response packet
            char payload[512];
            int payload_size = snprintf(payload, sizeof(payload), "~ HOST : %s ~ TIME : %s ~ CPU LOAD : %.2f%% ~", host, time_str, load);

            char response[MAX_BUFFER];
            memset(response, 0, MAX_BUFFER);

            struct iphdr *resp_ip = (struct iphdr *)response;
            resp_ip->ihl = 5;
            resp_ip->version = 4;
            resp_ip->tos = 0;
            resp_ip->tot_len = htons(sizeof(struct iphdr) + sizeof(CLDPHeader) + payload_size);
            resp_ip->id = htons(rand() % 65535);
            resp_ip->frag_off = 0;
            resp_ip->ttl = 64;
            resp_ip->protocol = CLDP;
            resp_ip->check = 0;
            // resp_ip->saddr = server_addr.sin_addr.s_addr;
            resp_ip->saddr = get_local_ip();
            resp_ip->daddr = header->saddr;

            CLDPHeader *response_cldp = (CLDPHeader *)(response + sizeof(struct iphdr));
            response_cldp->type = RESPONSE;
            response_cldp->payload = payload_size;
            response_cldp->tid = pack_header->tid;
            response_cldp->evenbit = 0;

            memcpy(response + sizeof(struct iphdr) + sizeof(CLDPHeader), payload, payload_size);

            resp_ip->check = checksum((uint16_t *)resp_ip, resp_ip->ihl * 2);

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_addr.s_addr = resp_ip->daddr;

            if (sendto(sockfd, response, ntohs(resp_ip->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
                perror("sendto failed\n");
            else
                printf("Sent RESPONSE to %s\n", client_ip);
        }
    }
    pthread_join(hello_thread, NULL);
    close(sockfd);

    return 0;
}