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
#include <net/if.h>
#include <time.h>
#include <netdb.h>

#define CLDP 253
#define HELLO 1
#define QUERY 2
#define RESPONSE 3
#define MAX_BUFFER 2048
#define SERVER_TIMEOUT 20
#define TIMEOUT_CHECK_INTERVAL 5
#define QUERY_INTERVAL 10
#define RESPONSE_WAIT_TIME 1
#define POLL_INTERVAL_USEC 100000

// header format custom
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t payload;
    uint16_t tid;
    uint32_t evenbit; // redundant
} CLDPHeader;

typedef struct server_node
{
    struct sockaddr_in address;
    time_t last_hello;
    struct server_node *next;
} server_node;

int sockfd;
server_node *server_list = NULL;
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
uint16_t transaction_id = 1;

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

void *query_thread_handler(void *args)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("Failed to set non-blocking mode");
        pthread_exit(NULL);
    }

    char hostname[512];
    if (gethostname(hostname, sizeof(hostname)) < 0)
    {
        perror("Failed to get hostname");
        pthread_exit(NULL);
    }
    struct hostent *host_info = gethostbyname(hostname);
    if (host_info == NULL)
    {
        perror("Failed to resolve hostname");
        pthread_exit(NULL);
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);

    uint16_t transaction_id = 0;
    while (1)
    {
        sleep(QUERY_INTERVAL);
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < RESPONSE_WAIT_TIME)
        {
            char buffer[MAX_BUFFER];
            memset(buffer, 0, MAX_BUFFER);
            struct sockaddr_in resp_addr;
            socklen_t len = sizeof(resp_addr);

            ssize_t n = recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&resp_addr, &len);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    usleep(POLL_INTERVAL_USEC);
                    continue;
                }
                else
                {
                    perror("Failed to receive RESPONSE");
                    break;
                }
            }

            struct iphdr *header = (struct iphdr *)buffer;
            if (header->protocol != CLDP)
                continue;
            int header_len = header->ihl * 4;
            if (n < header_len + sizeof(CLDPHeader))
                continue;
            CLDPHeader *cldp = (CLDPHeader *)(buffer + header_len);
            if (cldp->type == RESPONSE && ntohs(cldp->tid) == transaction_id)
            {
                int payload_len = cldp->payload;
                if (n >= header_len + sizeof(CLDPHeader) + payload_len)
                {
                    char payload[512];
                    memset(payload, 0, 512);
                    memcpy(payload, buffer + header_len + sizeof(CLDPHeader), payload_len);
                    char server_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(resp_addr.sin_addr), server_ip, INET_ADDRSTRLEN);
                    printf("Received RESPONSE from %s : %s\n", server_ip, payload);
                }
            }
        }
        pthread_mutex_lock(&server_mutex);

        if (!server_list)
        {
            pthread_mutex_unlock(&server_mutex);
            printf("Waiting for a server...\n");
            continue;
        }
        server_node *servers_cpy = NULL;
        server_node **copy_tail = &servers_cpy;
        server_node *curr = server_list;
        while (curr)
        {
            server_node *temp = (server_node *)malloc(sizeof(server_node));
            if (!temp)
            {
                perror("Memory allocation failed");
                break;
            }
            *temp = *curr;
            temp->next = NULL;
            *copy_tail = temp;
            copy_tail = &(temp->next);
            curr = curr->next;
        }
        pthread_mutex_unlock(&server_mutex);

        transaction_id++;
        server_node *iter = servers_cpy;
        while (iter)
        {
            char packet[MAX_BUFFER];
            memset(packet, 0, MAX_BUFFER);

            struct iphdr *ip = (struct iphdr *)packet;

            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(CLDPHeader));
            ip->id = htons(rand() % 65535);
            ip->frag_off = 0;
            ip->ttl = 64;
            ip->protocol = CLDP;
            ip->saddr = get_local_ip();
            ip->daddr = iter->address.sin_addr.s_addr;
            ip->check = 0;

            CLDPHeader *cldp = (CLDPHeader *)(packet + sizeof(struct iphdr));
            cldp->type = QUERY;
            cldp->payload = 0;
            cldp->tid = htons(transaction_id);
            cldp->evenbit = 0;

            ip->check = checksum((uint16_t *)ip, ip->ihl * 2);

            if (sendto(sockfd, packet, ntohs(ip->tot_len), 0, (struct sockaddr *)&(iter->address), sizeof(iter->address)) < 0)
            {
                perror("Failed to send QUERY");
            }
            else
            {
                char server_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(iter->address.sin_addr), server_ip, INET_ADDRSTRLEN);
                printf("Sent QUERY to %s (tid : %u)\n", server_ip, transaction_id);
            }

            iter = iter->next;
        }

        while (servers_cpy)
        {
            server_node *temp = servers_cpy;
            servers_cpy = servers_cpy->next;
            free(temp);
        }
    }
    pthread_exit(NULL);
}

void *hello_thread_handler(void *args)
{
    int listen_sock = socket(AF_INET, SOCK_RAW, CLDP);
    if (listen_sock < 0)
    {
        perror("Failed to create HELLO listener socket");
        pthread_exit(NULL);
    }
    int one = 1;
    if (setsockopt(listen_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("Failed to set IP_HDRINCL on listener");
        close(listen_sock);
        pthread_exit(NULL);
    }

    char buffer[MAX_BUFFER];
    while (1)
    {
        memset(buffer, 0, MAX_BUFFER);
        struct sockaddr_in src_addr;
        socklen_t len = sizeof(src_addr);

        ssize_t n = recvfrom(listen_sock, buffer, MAX_BUFFER, 0, (struct sockaddr *)&src_addr, &len);
        if (n < 0)
        {
            perror("Failed to receive HELLO message");
            continue;
        }

        struct iphdr *header = (struct iphdr *)buffer;
        if (header->protocol != CLDP)
            continue;
        int header_len = header->ihl * 4;
        if (n < header_len + sizeof(CLDPHeader))
            continue;

        CLDPHeader *cldp = (CLDPHeader *)(buffer + header_len);
        if (cldp->type == HELLO)
        {
            time_t curr_time = time(NULL);
            pthread_mutex_lock(&server_mutex);

            int flag = 0;
            server_node *curr = server_list;
            while (curr)
            {
                if (curr->address.sin_addr.s_addr == src_addr.sin_addr.s_addr)
                {
                    curr->last_hello = curr_time;
                    flag = 1;
                    break;
                }
                curr = curr->next;
            }

            if (!flag)
            {
                server_node *new_server = (server_node *)malloc(sizeof(server_node));
                if (!new_server)
                {
                    perror("Memory allocation failed");
                    pthread_mutex_unlock(&server_mutex);
                    continue;
                }

                new_server->address = src_addr;
                new_server->last_hello = curr_time;
                new_server->next = server_list;
                server_list = new_server;

                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(src_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                printf("New Server Detected : %s\n", ip_str);
            }

            pthread_mutex_unlock(&server_mutex);
        }
    }
    close(listen_sock); // Fixed: close the correct socket
    pthread_exit(NULL);
}

void *timeout_thread_handler(void *args)
{
    while (1)
    {
        sleep(TIMEOUT_CHECK_INTERVAL);

        pthread_mutex_lock(&server_mutex);
        time_t curr_time = time(NULL);
        server_node **temp = &server_list;

        while (*temp)
        {
            server_node *curr = *temp;
            if (curr_time - curr->last_hello > SERVER_TIMEOUT)
            {
                *temp = curr->next;
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(curr->address.sin_addr), ip_str, INET_ADDRSTRLEN);
                printf("Server %s timed out and removed\n", ip_str);
                free(curr);
            }
            else
                temp = &(curr->next);
        }

        pthread_mutex_unlock(&server_mutex);
    }

    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    printf("CLDP Client Starting...\n");
    srand(time(NULL));

    sockfd = socket(AF_INET, SOCK_RAW, CLDP);
    if (sockfd < 0)
    {
        perror("Failed to create socket");
        return 1;
    }
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("Failed to set IP_HDRINCL on listener");
        close(sockfd);
        return 1;
    }

    pthread_t hello_thread;
    if (pthread_create(&hello_thread, NULL, hello_thread_handler, NULL) != 0)
    {
        perror("Failed to create hello listener thread");
        close(sockfd);
        return 1;
    }
    pthread_t query_thread;
    if (pthread_create(&query_thread, NULL, query_thread_handler, NULL) != 0)
    {
        perror("Failed to create query processor thread");
        close(sockfd);
        return 1;
    }
    pthread_t timeout_thread;
    if (pthread_create(&timeout_thread, NULL, timeout_thread_handler, NULL) != 0)
    {
        perror("Failed to create timeout checker thread");
        close(sockfd);
        return 1;
    }
    printf("Client Started...\n");

    pthread_join(hello_thread, NULL);
    pthread_join(query_thread, NULL);
    pthread_join(timeout_thread, NULL);

    close(sockfd);
    return 0;
}