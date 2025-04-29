#include "ksocket.h"

struct SM_data *SM;            // shared memory for socket data
struct SOCKET_INFO *sock_info; // shared memory for the socket information

// semaphores
int sem1, sem2;
int sem_sock_info, sem_SM;
int shmid_sock_info, shmid_SM;
struct sembuf pop, vop;

#define PATHNAME "/"

void get_shared_resources()
{
    int key_sem1, key_sem2;
    int key_sem_sock_info, key_sem_SM;
    int key_shmid_sock_info, key_shmid_SM;

    key_shmid_sock_info = ftok(PATHNAME, 'A');
    key_shmid_SM = ftok(PATHNAME, 'B');
    key_sem1 = ftok(PATHNAME, 'C');
    key_sem2 = ftok(PATHNAME, 'D');
    key_sem_SM = ftok(PATHNAME, 'E');
    key_sem_sock_info = ftok(PATHNAME, 'F');

    shmid_sock_info = shmget(key_shmid_sock_info, sizeof(struct SOCKET_INFO), 0666);
    shmid_SM = shmget(key_shmid_SM, sizeof(struct SM_data) * N, 0666);
    sem1 = semget(key_sem1, 1, 0666);
    sem2 = semget(key_sem2, 1, 0666);
    sem_SM = semget(key_sem_SM, 1, 0666);
    sem_sock_info = semget(key_sem_sock_info, 1, 0666);

    if (shmid_sock_info == -1 || shmid_SM == -1 || sem1 == -1 || sem2 == -1 || sem_SM == -1 || sem_sock_info == -1)
    {
        perror("Error in getting shared resources");
        exit(1);
    }

    SM = (struct SM_data *)shmat(shmid_SM, 0, 0);
    sock_info = (struct SOCKET_INFO *)shmat(shmid_sock_info, 0, 0);

    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;

    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
}

int return_free_entry()
{
    P(sem_SM); // for atomicity we have used semaphores at many places
    for (int i = 0; i < N; i++)
    {
        if (SM[i].is_free == 1)
        {
            V(sem_SM);
            return i;
        }
    }
    V(sem_SM);
    return -1;
}

// create a new KTP socket
int k_socket(int domain, int type, int protocol)
{
    get_shared_resources();

    if (type != SOCK_KTP)
    {
        errno = EINVAL;
        return -1;
    }

    int i;
    P(sem_sock_info);
    if ((i = return_free_entry()) == -1)
    {
        errno = ENOSPACE;
        sock_info->error_no = ENOSPACE;
        sock_info->sockid = 0;
        sock_info->error_no = 0;
        sock_info->ip_addr[0] = '\0';
        sock_info->port = 0;
        V(sem_sock_info);
        return -1;
    }

    V(sem_sock_info);
    V(sem1);
    P(sem2);
    P(sem_sock_info);

    if (sock_info->sockid == -1)
    {
        errno = sock_info->error_no;
        sock_info->sockid = 0;
        sock_info->error_no = 0;
        sock_info->ip_addr[0] = '\0';
        sock_info->port = 0;
        V(sem_sock_info);
        return -1;
    }

    V(sem_sock_info);

    // initialize the socket entry in the shared memory
    P(sem_SM);
    SM[i].is_free = 0;
    SM[i].pid = getpid();
    SM[i].udp_sockid = sock_info->sockid;

    for (int j = 0; j < MAX_SEQNO; j++)
    {
        SM[i].swnd.wndw[j] = -1;
        SM[i].last_sent_time[j] = -1;
        if (j > 0 && j < MAX_WINDOW_SIZE + 1)
            SM[i].rwnd.wndw[j] = j - 1;
        else
            SM[i].rwnd.wndw[j] = -1;
    }

    SM[i].swnd.size = MAX_WINDOW_SIZE;
    SM[i].rwnd.size = MAX_WINDOW_SIZE;
    SM[i].swnd.start_seq = 1;
    SM[i].rwnd.start_seq = 1;
    SM[i].send_buffer_size = MAX_WINDOW_SIZE;

    for (int j = 0; j < MAX_MSGS; j++)
        SM[i].receive_buffer_valid[j] = 0;

    SM[i].receive_buffer_pointer = 0;
    SM[i].nospace = 0;
    V(sem_SM);

    // reset the socket information
    P(sem_sock_info);
    sock_info->sockid = 0;
    sock_info->error_no = 0;
    sock_info->ip_addr[0] = '\0';
    sock_info->port = 0;
    V(sem_sock_info);

    return i;
}

// binder function for the KTP sockets
int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port)
{
    get_shared_resources();

    int sockfd = -1;
    P(sem_SM);

    for (int i = 0; i < N; i++)
    {
        if (SM[i].is_free == 0 && SM[i].pid == getpid())
        {
            sockfd = i;
            break;
        }
    }

    P(sem_sock_info);
    if (sockfd == -1 || sockfd == N)
    {
        errno = ENOSPACE;
        sock_info->sockid = 0;
        sock_info->error_no = 0;
        sock_info->ip_addr[0] = '\0';
        sock_info->port = 0;
        V(sem_SM);
        V(sem_sock_info);
        return -1;
    }

    sock_info->sockid = SM[sockfd].udp_sockid;
    strcpy(sock_info->ip_addr, src_ip);
    sock_info->port = src_port;
    V(sem_sock_info);
    V(sem1);
    P(sem2);
    P(sem_sock_info);

    if (sock_info->sockid == -1)
    {
        errno = sock_info->error_no;
        sock_info->sockid = 0;
        sock_info->error_no = 0;
        sock_info->ip_addr[0] = '\0';
        sock_info->port = 0;
        V(sem_sock_info);
        V(sem_SM);
        return -1;
    }

    V(sem_sock_info);
    strcpy(SM[sockfd].ip_addr, dest_ip);
    SM[sockfd].port = dest_port;
    V(sem_SM);

    P(sem_sock_info);
    sock_info->sockid = 0;
    sock_info->error_no = 0;
    sock_info->ip_addr[0] = '\0';
    sock_info->port = 0;
    V(sem_sock_info);

    return 0;
}

// custom sendto function for KTP sockets
ssize_t k_sendto(int k_sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    get_shared_resources();

    P(sem_SM);
    char *dest_ip;
    uint16_t dest_port;

    dest_ip = inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr);
    dest_port = ntohs(((struct sockaddr_in *)dest_addr)->sin_port);

    if (strcmp(SM[k_sockfd].ip_addr, dest_ip) != 0 || SM[k_sockfd].port != dest_port)
    {
        errno = ENOBOUND;
        V(sem_SM);
        return -1;
    }

    if (SM[k_sockfd].send_buffer_size == 0)
    {
        errno = ENOSPACE;
        V(sem_SM);
        return -1;
    }

    int seq_no = SM[k_sockfd].swnd.start_seq;
    while (SM[k_sockfd].swnd.wndw[seq_no] != -1)
    {
        seq_no = (seq_no + 1) % MAX_SEQNO;
    }

    int buffer_index = 0, f = 1;
    for (buffer_index = 0; buffer_index < MAX_MSGS; buffer_index++)
    {
        f = 1;
        for (int i = 0; i < 256; i++)
        {
            if (SM[k_sockfd].swnd.wndw[i] == buffer_index)
            {
                f = 0;
                break;
            }
        }
        if (f == 1)
            break;
    }

    if (f == 0)
    {
        errno = ENOSPACE;
        V(sem_SM);
        return -1;
    }

    SM[k_sockfd].swnd.wndw[seq_no] = buffer_index;
    memcpy(SM[k_sockfd].send_buffer[buffer_index], buf, len); // copy the message to the buffer
    SM[k_sockfd].len_of_msg_send_buffer[buffer_index] = len;
    SM[k_sockfd].send_buffer_size--;
    SM[k_sockfd].last_sent_time[seq_no] = -1;

    V(sem_SM);
    return len;
}

ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    get_shared_resources();

    P(sem_SM);
    if (sockfd < 0 || sockfd >= N || SM[sockfd].is_free)
    {
        errno = EBADF;
        V(sem_SM);
        return -1;
    }

    struct SM_data *sm = SM + sockfd;
    if (sm->receive_buffer_valid[sm->receive_buffer_pointer])
    {
        sm->receive_buffer_valid[sm->receive_buffer_pointer] = 0;
        sm->rwnd.size++;

        int seq = -1;
        for (int i = 0; i < MAX_SEQNO; i++)
            if (sm->rwnd.wndw[i] == sm->receive_buffer_pointer)
                seq = i;

        sm->rwnd.wndw[seq] = -1;
        sm->rwnd.wndw[(seq + MAX_MSGS) % MAX_SEQNO] = sm->receive_buffer_pointer;

        int length = sm->len_of_msg_receive_buffer[sm->receive_buffer_pointer];
        memcpy(buf, sm->receive_buffer[sm->receive_buffer_pointer], (len < length) ? len : length);

        sm->receive_buffer_pointer = (sm->receive_buffer_pointer + 1) % MAX_MSGS;

        V(sem_SM);
        return (len < length) ? len : length;
    }

    errno = ENOMESSAGE;
    V(sem_SM);
    return -1;
}

int k_close(int sockfd)
{
    get_shared_resources();

    P(sem_SM);
    if (sockfd < 0 || sockfd >= N || SM[sockfd].is_free)
    {
        errno = EBADF;
        V(sem_SM);
        return -1;
    }

    SM[sockfd].is_free = 1;
    V(sem_SM);
    return 0;
}

int drop_message()
{
    float r = (float)rand() / (float)RAND_MAX;
    if (r < PROB)
        return 1;
    return 0;
}