/*
==============================
Assignment 4 Final Submission
Name: Omkar Bhandare
Roll Number: 22CS30016
==============================
*/
#include "ksocket.h"

void lock_socket(sem_t *mutex)
{
    if (sem_wait(mutex) != 0)
    {
        perror("Semaphore lock failed");
        exit(EXIT_FAILURE);
    }
}

void unlock_socket(sem_t *mutex)
{
    if (sem_post(mutex) != 0)
    {
        perror("Semaphore unlock failed");
        exit(EXIT_FAILURE);
    }
    sem_close(mutex);
}

SMData *get_shm(void)
{
    key_t key = ftok(".", 'A');
    int shmid = shmget(key, sizeof(SMData), 0666);

    if (shmid < 0)
    {
        perror("error shmget");
        exit(EXIT_FAILURE);
    }

    SMData *shm = (SMData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1)
    {
        perror("error shmat");
        exit(EXIT_FAILURE);
    }

    return shm;
}

void initialize_buffer(struct buffer *buf)
{
    buf->filled = 0;
    buf->iter = 0;

    for (int i = 0; i < MAXMESSAGE; i++)
    {
        buf->isempty[i] = 1;
        memset(buf->buff[i], 0, MESSAGELEN);
    }
}

void sockreset(ktp_socket *sock)
{
    sock->in_use = 0;
    sock->is_closed = 0;
    sock->is_bound = 0;
    sock->pid = 0;

    if (sock->sockfd >= 0)
    {
        close(sock->sockfd);
    }
    sock->sockfd = -1;

    sock->dest.ip = 0;
    sock->dest.port = 0;
    sock->src.ip = 0;
    sock->src.port = 0;

    sock->swnd.size = MAXMESSAGE;
    sock->swnd.head = 0;
    sock->swnd.tail = 0;

    sock->rwnd.size = MAXMESSAGE;
    sock->rwnd.head = 0;
    sock->rwnd.tail = MAXMESSAGE;
    sock->last_acked = 0;
    sock->nospace = 0;

    initialize_buffer(&sock->send_buffer);
    initialize_buffer(&sock->recv_buffer);
}

int check_socket_validity(int sockfd, SMData *shm, sem_t *mutex)
{
    if (sockfd < 0 || sockfd >= MAXSOCKETS)
    {
        unlock_socket(mutex);
        errno = EINVAL;
        return 0;
    }

    if (shm->sockets[sockfd].in_use == 0)
    {
        unlock_socket(mutex);
        errno = EINVAL;
        return 0;
    }

    return 1;
}

int k_socket(int domain, int type, int protocol)
{
    if (type != SOCK_KTP)
    {
        errno = EINVAL;
        return -1;
    }

    SMData *shm = get_shm();
    int free_space = -1;
    sem_t *mutex;
    int i = 0;

    do
    {
        mutex = sem_open(shm->sockets[i].socket_mutex, O_CREAT, 0666, 1);
        lock_socket(mutex);

        if (shm->sockets[i].in_use == 0)
        {
            free_space = i;
            unlock_socket(mutex);
            break;
        }

        unlock_socket(mutex);
        i++;
    } while (i < MAXSOCKETS);

    if (free_space == -1)
    {
        errno = ENOSPACE;
        return -1;
    }

    mutex = sem_open(shm->sockets[free_space].socket_mutex, O_CREAT, 0666, 1);
    lock_socket(mutex);

    sockreset(&(shm->sockets[free_space]));

    shm->sockets[free_space].in_use = 1;
    shm->sockets[free_space].pid = getpid();

    unlock_socket(mutex);
    return free_space;
}

ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen)
{
    if (len != MESSAGELEN)
    {
        errno = EINVAL;
        return -1;
    }

    SMData *shm = get_shm();
    sem_t *mutex = sem_open(shm->sockets[sockfd].socket_mutex, O_CREAT, 0666, 1);
    lock_socket(mutex);

    if (!check_socket_validity(sockfd, shm, mutex))
        return -1;

    if (shm->sockets[sockfd].dest.ip != ((struct sockaddr_in *)addr)->sin_addr.s_addr ||
        shm->sockets[sockfd].dest.port != ((struct sockaddr_in *)addr)->sin_port)
    {
        unlock_socket(mutex);
        errno = ENOTBOUND;
        return -1;
    }

    if (shm->sockets[sockfd].send_buffer.filled == MAXMESSAGE)
    {
        unlock_socket(mutex);
        errno = ENOSPACE;
        return -1;
    }

    int buffer_idx = shm->sockets[sockfd].send_buffer.iter;
    memcpy(shm->sockets[sockfd].send_buffer.buff[buffer_idx], buf, MESSAGELEN);

    shm->sockets[sockfd].send_buffer.isempty[buffer_idx] = 0;
    shm->sockets[sockfd].send_buffer.iter = (buffer_idx + 1) % MAXMESSAGE;
    shm->sockets[sockfd].send_buffer.filled++;

    unlock_socket(mutex);
    return len;
}

int k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
    SMData *shm = get_shm();
    sem_t *mutex = sem_open(shm->sockets[sockfd].socket_mutex, O_CREAT, 0666, 1);
    lock_socket(mutex);

    if (!check_socket_validity(sockfd, shm, mutex))
        return -1;

    if (shm->sockets[sockfd].recv_buffer.filled == 0 ||
        shm->sockets[sockfd].recv_buffer.isempty[shm->sockets[sockfd].recv_buffer.iter])
    {
        unlock_socket(mutex);
        errno = ENOMESSAGE;
        return -1;
    }

    int buffer_idx = shm->sockets[sockfd].recv_buffer.iter;
    memcpy(buf, shm->sockets[sockfd].recv_buffer.buff[buffer_idx], MESSAGELEN);

    shm->sockets[sockfd].recv_buffer.isempty[buffer_idx] = 1;
    shm->sockets[sockfd].recv_buffer.iter = (buffer_idx + 1) % MAXMESSAGE;
    shm->sockets[sockfd].recv_buffer.filled--;

    shm->sockets[sockfd].rwnd.tail = (shm->sockets[sockfd].rwnd.tail + 1) % SEQNUM;

    unlock_socket(mutex);
    return len;
}

int k_bind(int sockfd, in_addr_t src_ip, in_port_t src_port, in_addr_t dest_ip, in_port_t dest_port)
{
    SMData *shm = get_shm();
    sem_t *mutex = sem_open(shm->sockets[sockfd].socket_mutex, O_CREAT, 0666, 1);
    lock_socket(mutex);

    if (!check_socket_validity(sockfd, shm, mutex))
        return -1;

    shm->sockets[sockfd].src.ip = src_ip;
    shm->sockets[sockfd].src.port = src_port;
    shm->sockets[sockfd].dest.ip = dest_ip;
    shm->sockets[sockfd].dest.port = dest_port;

    unlock_socket(mutex);

    shm->sockets[sockfd].is_bound = 1;
    return 0;
}

int k_close(int sockfd)
{
    SMData *shm = get_shm();
    sem_t *mutex = sem_open(shm->sockets[sockfd].socket_mutex, O_CREAT, 0666, 1);
    lock_socket(mutex);

    if (!check_socket_validity(sockfd, shm, mutex))
        return -1;

    shm->sockets[sockfd].is_closed = 1;

    unlock_socket(mutex);
    return 0;
}