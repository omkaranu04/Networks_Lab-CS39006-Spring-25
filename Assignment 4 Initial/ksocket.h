#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/select.h>

#define SOCK_KTP 100

#define T 5                // timeout period
#define PROB 0.01          // packet drop probability
#define N 5                // maximum entries at a time in the shared memory
#define MAX_MSGS 10        // maximum no. of messages
#define MSG_SIZE 512       // maximum size of a message
#define MAX_SEQNO 256      // 8 bit sequence number
#define MAX_WINDOW_SIZE 10 // rwnd and swnd size maximum

// custom defined error codes
#define ENOSPACE 9000
#define ENOBOUND 9001
#define ENOMESSAGE 9002

// semaphores for shared resources
#define P(s) semop(s, &pop, 1) // wait operation
#define V(s) semop(s, &vop, 1) // signal operation

struct window
{
    int wndw[256]; // sequence numbers
    int size;      // current window size
    int start_seq; // starting sequence number
};

struct SM_data
{
    int is_free;                             // if the socket is free
    pid_t pid;                               // process id of the owner process
    int udp_sockid;                          // the OG UDP socket of the underlying implementation
    char ip_addr[16];                        // ip address
    uint16_t port;                           // port number
    char send_buffer[MAX_MSGS][MSG_SIZE];    // send buffer
    int send_buffer_size;                    // current size of the send buffer
    int len_of_msg_send_buffer[MAX_MSGS];    // length of each message in the send buffer
    char receive_buffer[MAX_MSGS][MSG_SIZE]; // receive buffer
    int receive_buffer_pointer;              // pointer to the next message to be read
    int receive_buffer_valid[MAX_MSGS];      // validity of incoming messages
    int len_of_msg_receive_buffer[MAX_MSGS]; // length of each message in the receive buffer
    struct window swnd;                      // sender window
    struct window rwnd;                      // receiver window
    int nospace;                             // flag for buffer full condition
    time_t last_sent_time[256];              // timestamp tracker
};

struct SOCKET_INFO
{
    int sockid;       // socket number
    char ip_addr[16]; // ip address
    uint16_t port;    // port number
    int error_no;     // error number
};

extern struct SM_data *SM;            // shared memory for socket data
extern struct SOCKET_INFO *sock_info; // shared memory for socket info
extern int sem1, sem2;                // semaphores for socket creation
extern int sem_sock_info, sem_SM;     // semaphores for shared memory
extern int shmid_sock_info, shmid_SM; // shared memory ids
extern struct sembuf pop, vop;        // semaphore operations

// custom defined functions equivalent to the UDP socket implementation
int k_socket(int domain, int type, int protocol);
int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);

// to simulate the lost packets
int drop_message();

// helper function to get shared resources
void get_shared_resources();

#endif