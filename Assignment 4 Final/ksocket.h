/*
==============================
Assignment 4 Final Submission
Name: Omkar Bhandare
Roll Number: 22CS30016
==============================
*/
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
#include <pthread.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <inttypes.h>
#include <time.h>

#define HEADERSIZE 4   // header size
#define MESSAGELEN 512 // maximum message length exclusive of header
#define MAXMESSAGE 10  // maximum number of messages in buffer
#define MAXSOCKETS 5   // maximum sockets that can be handled in system

#define p 0         // message drop probability
#define T 10        // retransmission timeout
#define GCTIMEOUT 1 // garbage collection timeout
#define SEQNUM 256  // sequence number range (starts from 1 as mentioned)

// can be used interchangeably
#define SOCK_KTP 16 // socket type for KTP protocol
#define KTP_SOCK 16 // alternative name for SOCK_KTP

// custom error codes
#define ENOSPACE 9000   // error: no space available in buffer
#define ENOTBOUND 9001  // error: socket not bound
#define ENOMESSAGE 9002 // error: no message available to receive

struct window
{
    int size;       // maximum window size
    int head, tail; // head and tail pointers for sliding window
};

struct buffer
{
    char buff[MAXMESSAGE][MESSAGELEN]; // array of message buffers
    int filled;                        // current number of messages in buffer
    int iter;                          // current buffer position
    int isempty[MAXMESSAGE];           // tracks which buffer slots are empty
};

struct ip_port
{
    uint32_t ip;   // IP address
    uint16_t port; // port number
};

typedef struct ktp_socket
{
    int in_use;    // bool type for if socket is allocated
    int is_bound;  // bool type for if socket is bound
    int is_closed; // bool type for if socket is closed

    int sockfd; // UDP sockfd that is used for communication
    int pid;    // process which owns this socket

    struct ip_port dest; // destination address
    struct ip_port src;  // source address

    struct buffer send_buffer; // sender buffer
    struct buffer recv_buffer; // receiver buffer

    struct window swnd; // sender window
    struct window rwnd; // receiver window

    int last_acked;        // last acknowledged sequence number
    int nospace;           // flag for no space in buffer
    char socket_mutex[10]; // name of semaphore for synchronization
} ktp_socket;

typedef struct
{
    struct ktp_socket sockets[MAXSOCKETS]; // array of sockets in shared memory
} SMData;

SMData *get_shm();
void sockreset(ktp_socket *sock);
int check_socket_validity(int sockfd, SMData *shm, sem_t *mutex);
int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, in_addr_t src_ip, in_port_t src_port, in_addr_t dest_ip, in_port_t dest_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen);
int k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen);
int k_close(int sockfd);
