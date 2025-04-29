#include "ksocket.h"

#define PATHNAME "/"

/*
TODO for later submission
1. Implement the sliding window properly, the sliding window mechanism is not assured to be working properly
2. Currently there is some corruption in the bits while transferring, need to pay attention to that, maybe due to incorrect semaphore usage
3. The window size at the sender end needs to be dynamic according to the buffer size of the receiver that is still pending to be implemented
4. The packet structure can be improved (if time permits)
*/

void signal_handler(int signum);
void *receiver_thread_handler();
void *sender_thread_handler();
void *garbage_collector_thread_handler();

/*
Each message will have a 9 bit header. 1 bit for type (0 = ACK, 1 = DATA), 8 bit for sequence number.
Next bits are either 4-bit rwnd size (for ACK) or 10 bits of length and 512 bytes data (for DATA).
*/

int total_transmissions;

int main(int argc, char const *argv[])
{
    total_transmissions = 0;
    signal(SIGINT, signal_handler);
    srand(time(0));

    // Initialize semaphore operations
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;

    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;

    // Generate keys for shared resources
    int key_sem1, key_sem2;
    int key_sem_sock_info, key_sem_SM;
    int key_shmid_sock_info, key_shmid_SM;

    key_shmid_sock_info = ftok(PATHNAME, 'A');
    key_shmid_SM = ftok(PATHNAME, 'B');
    key_sem1 = ftok(PATHNAME, 'C');
    key_sem2 = ftok(PATHNAME, 'D');
    key_sem_SM = ftok(PATHNAME, 'E');
    key_sem_sock_info = ftok(PATHNAME, 'F');

    // Create shared memory segments and semaphores
    shmid_sock_info = shmget(key_shmid_sock_info, sizeof(struct SOCKET_INFO), IPC_CREAT | 0777);
    shmid_SM = shmget(key_shmid_SM, sizeof(struct SM_data) * N, IPC_CREAT | 0777);
    sem1 = semget(key_sem1, 1, IPC_CREAT | 0777);
    sem2 = semget(key_sem2, 1, IPC_CREAT | 0777);
    sem_SM = semget(key_sem_SM, 1, IPC_CREAT | 0777);
    sem_sock_info = semget(key_sem_sock_info, 1, IPC_CREAT | 0777);

    // Attach to shared memory
    sock_info = (struct SOCKET_INFO *)shmat(shmid_sock_info, 0, 0);
    SM = (struct SM_data *)shmat(shmid_SM, 0, 0);

    if (shmid_sock_info == -1 || shmid_SM == -1 || sem1 == -1 || sem2 == -1 || sem_SM == -1 || sem_sock_info == -1)
    {
        perror("Error in getting shared resources");
        exit(1);
    }

    // Initialize socket info
    sock_info->sockid = 0;
    sock_info->error_no = 0;
    sock_info->ip_addr[0] = '\0';
    sock_info->port = 0;

    // Initialize all sockets as free
    for (int i = 0; i < N; i++)
        SM[i].is_free = 1;

    // Initialize semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(sem_SM, 0, SETVAL, 1);
    semctl(sem_sock_info, 0, SETVAL, 1);

    // Create threads for handling socket operations
    pthread_t receiver_thread, sender_thread, garbage_collector_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&receiver_thread, &attr, receiver_thread_handler, NULL);
    pthread_create(&sender_thread, &attr, sender_thread_handler, NULL);
    pthread_create(&garbage_collector_thread, &attr, garbage_collector_thread_handler, NULL);

    // Main loop for handling socket creation and binding
    while (1)
    {
        P(sem1);
        P(sem_sock_info);

        if (sock_info->sockid == 0 && sock_info->ip_addr[0] == '\0' && sock_info->port == 0)
        {
            // Create a new UDP socket
            int sockid = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockid == -1)
            {
                sock_info->sockid = -1;
                sock_info->error_no = errno;
            }
            else
            {
                sock_info->sockid = sockid;
            }
        }
        else
        {
            // Bind the socket to the specified address
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(sock_info->port);
            server_addr.sin_addr.s_addr = inet_addr(sock_info->ip_addr);

            if (bind(sock_info->sockid, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                sock_info->sockid = -1;
                sock_info->error_no = errno;
            }
        }

        V(sem_sock_info);
        V(sem2);
    }

    return 0;
}

void signal_handler(int signum)
{
    // Clean up shared resources on exit
    shmdt(sock_info);
    shmdt(SM);
    shmctl(shmid_sock_info, IPC_RMID, 0);
    shmctl(shmid_SM, IPC_RMID, 0);
    semctl(sem1, 0, IPC_RMID);
    semctl(sem2, 0, IPC_RMID);
    semctl(sem_SM, 0, IPC_RMID);
    semctl(sem_sock_info, 0, IPC_RMID);
    exit(0);
}

void *garbage_collector_thread_handler()
{
    while (1)
    {
        sleep(T);
        P(sem_SM);

        for (int i = 0; i < N; i++)
        {
            if (SM[i].is_free)
                continue;

            // Check if process still exists
            if (kill(SM[i].pid, 0) == 0)
                continue;

            // Free the socket if process doesn't exist
            SM[i].is_free = 1;
        }

        V(sem_SM);
    }
}

void *receiver_thread_handler()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    int nfds = 0;

    while (1)
    {
        fd_set readyfds = readfds;
        struct timeval timeout;
        timeout.tv_sec = T;
        timeout.tv_usec = 0;

        int retval = select(nfds + 1, &readyfds, NULL, NULL, &timeout);
        if (retval == -1)
            perror("select()");

        if (retval <= 0)
        {
            // Timeout or error, rebuild the fd set
            FD_ZERO(&readfds);
            nfds = 0;

            P(sem_SM);
            for (int i = 0; i < N; i++)
            {
                if (SM[i].is_free == 0)
                {
                    FD_SET(SM[i].udp_sockid, &readfds);
                    if (SM[i].udp_sockid > nfds)
                        nfds = SM[i].udp_sockid;

                    // Send window update if previously had no space but now has space
                    if (SM[i].nospace && SM[i].rwnd.size > 0)
                    {
                        SM[i].nospace = 0;

                        int lastseq = ((SM[i].rwnd.start_seq + MAX_SEQNO) - 1) % MAX_SEQNO;
                        struct sockaddr_in client_addr;
                        client_addr.sin_family = AF_INET;
                        client_addr.sin_addr.s_addr = inet_addr(SM[i].ip_addr);
                        client_addr.sin_port = htons(SM[i].port);

                        char ack[13];
                        ack[0] = '0'; // type

                        // 8 bit sequence number
                        for (int j = 0; j < 8; j++)
                        {
                            ack[j + 1] = ((lastseq >> (7 - j)) & 1) + '0';
                        }

                        // 4 bit rwnd size
                        for (int j = 0; j < 4; j++)
                        {
                            ack[j + 9] = ((SM[i].rwnd.size >> (3 - j)) & 1) + '0';
                        }

                        sendto(SM[i].udp_sockid, ack, 13, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                    }
                }
            }
            V(sem_SM);
        }
        else
        {
            // Data is available to read
            P(sem_SM);
            for (int i = 0; i < N; i++)
            {
                if (FD_ISSET(SM[i].udp_sockid, &readyfds))
                {
                    char buffer[MSG_SIZE + 16];
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);

                    int n = recvfrom(SM[i].udp_sockid, buffer, MSG_SIZE + 16, 0, (struct sockaddr *)&client_addr, &addr_len);

                    // Simulate packet loss
                    if (drop_message())
                        continue;

                    if (n < 0)
                    {
                        perror("recvfrom()");
                    }
                    else
                    {
                        if (buffer[0] == '0')
                        {
                            // ACK packet
                            int seq = 0;
                            for (int j = 0; j < 8; j++)
                            {
                                seq = seq * 2 + (buffer[j + 1] - '0');
                            }

                            int rwnd = 0;
                            for (int j = 0; j < 4; j++)
                            {
                                rwnd = rwnd * 2 + (buffer[j + 9] - '0');
                            }

                            // Update sender window if ACK is valid
                            if (SM[i].swnd.wndw[seq] >= 0)
                            {
                                int j = SM[i].swnd.start_seq;
                                while (j != (seq + 1) % MAX_SEQNO)
                                {
                                    SM[i].swnd.wndw[j] = -1;
                                    SM[i].last_sent_time[j] = -1;
                                    SM[i].send_buffer_size++;
                                    j = (j + 1) % MAX_SEQNO;
                                }

                                SM[i].swnd.start_seq = (seq + 1) % MAX_SEQNO;
                            }

                            // Update window size based on receiver's advertised window
                            SM[i].swnd.size = rwnd;
                        }
                        else
                        {
                            // DATA packet
                            int seq = 0;
                            for (int j = 0; j < 8; j++)
                            {
                                seq = seq * 2 + (buffer[j + 1] - '0');
                            }

                            int len = 0;
                            for (int j = 0; j < 10; j++)
                            {
                                len = len * 2 + (buffer[j + 9] - '0');
                            }

                            // Process in-order packet
                            if (seq == SM[i].rwnd.start_seq)
                            {
                                int buffer_index = SM[i].rwnd.wndw[seq];
                                if (buffer_index >= 0)
                                {
                                    memcpy(SM[i].receive_buffer[buffer_index], buffer + 19, len);
                                    SM[i].receive_buffer_valid[buffer_index] = 1;
                                    SM[i].rwnd.size--;
                                    SM[i].len_of_msg_receive_buffer[buffer_index] = len;

                                    // Advance window for consecutive valid packets
                                    while (SM[i].rwnd.wndw[SM[i].rwnd.start_seq] >= 0 &&
                                           SM[i].receive_buffer_valid[SM[i].rwnd.wndw[SM[i].rwnd.start_seq]] == 1)
                                    {
                                        SM[i].rwnd.start_seq = (SM[i].rwnd.start_seq + 1) % MAX_SEQNO;
                                    }
                                }
                            }
                            else
                            {
                                // Process out-of-order packet
                                if (SM[i].rwnd.wndw[seq] >= 0 && SM[i].receive_buffer_valid[SM[i].rwnd.wndw[seq]] == 0)
                                {
                                    int buffer_index = SM[i].rwnd.wndw[seq];
                                    memcpy(SM[i].receive_buffer[buffer_index], buffer + 19, len);
                                    SM[i].receive_buffer_valid[buffer_index] = 1;
                                    SM[i].rwnd.size--;
                                    SM[i].len_of_msg_receive_buffer[buffer_index] = len;
                                }
                            }

                            // Set no space flag if window is full
                            if (SM[i].rwnd.size == 0)
                                SM[i].nospace = 1;

                            // Send ACK
                            int ack_seq = (SM[i].rwnd.start_seq + MAX_SEQNO - 1) % MAX_SEQNO;
                            char ack[13];
                            ack[0] = '0'; // type

                            // 8 bit sequence number
                            for (int j = 0; j < 8; j++)
                            {
                                ack[j + 1] = ((ack_seq >> (7 - j)) & 1) + '0';
                            }

                            // 4 bit rwnd size
                            for (int j = 0; j < 4; j++)
                            {
                                ack[j + 9] = ((SM[i].rwnd.size >> (3 - j)) & 1) + '0';
                            }

                            sendto(SM[i].udp_sockid, ack, 13, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        }
                    }
                }
            }
            V(sem_SM);
        }
    }
}

void *sender_thread_handler()
{
    while (1)
    {
        sleep(T / 2);
        P(sem_SM);

        for (int i = 0; i < N; i++)
        {
            if (SM[i].is_free == 0)
            {
                struct sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(SM[i].port);
                server_addr.sin_addr.s_addr = inet_addr(SM[i].ip_addr);

                // Check for timeouts
                int timeout = 0;
                int j = SM[i].swnd.start_seq;
                while (j != (SM[i].swnd.start_seq + SM[i].swnd.size) % MAX_SEQNO)
                {
                    if (SM[i].last_sent_time[j] != -1 && time(NULL) - SM[i].last_sent_time[j] > T)
                    {
                        timeout = 1;
                        break;
                    }
                    j = (j + 1) % MAX_SEQNO;
                }

                if (timeout)
                {
                    // Retransmit all packets in window
                    j = SM[i].swnd.start_seq;
                    int start = SM[i].swnd.start_seq;
                    while (j != (start + SM[i].swnd.size) % MAX_SEQNO)
                    {
                        if (SM[i].swnd.wndw[j] != -1)
                        {
                            char buffer[MSG_SIZE + 16];
                            buffer[0] = '1'; // type

                            // 8 bit sequence number
                            for (int k = 0; k < 8; k++)
                            {
                                buffer[k + 1] = ((j >> (7 - k)) & 1) + '0';
                            }

                            int len = SM[i].len_of_msg_send_buffer[SM[i].swnd.wndw[j]];

                            // 10 bit length
                            for (int k = 0; k < 10; k++)
                            {
                                buffer[k + 9] = ((len >> (9 - k)) & 1) + '0';
                            }

                            memcpy(buffer + 19, SM[i].send_buffer[SM[i].swnd.wndw[j]], len);
                            sendto(SM[i].udp_sockid, buffer, len + 19, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                            SM[i].last_sent_time[j] = time(NULL);
                            total_transmissions++;
                        }
                        j = (j + 1) % MAX_SEQNO;
                    }
                }
                else
                {
                    // Send new packets
                    j = SM[i].swnd.start_seq;
                    int start = SM[i].swnd.start_seq;
                    while (j != (start + SM[i].swnd.size) % MAX_SEQNO)
                    {
                        if (SM[i].swnd.wndw[j] != -1 && SM[i].last_sent_time[j] == -1)
                        {
                            char buffer[MSG_SIZE + 16];
                            buffer[0] = '1'; // type

                            // 8 bit sequence number
                            for (int k = 0; k < 8; k++)
                            {
                                buffer[k + 1] = ((j >> (7 - k)) & 1) + '0';
                            }

                            int len = SM[i].len_of_msg_send_buffer[SM[i].swnd.wndw[j]];

                            // 10 bit length
                            for (int k = 0; k < 10; k++)
                            {
                                buffer[k + 9] = ((len >> (9 - k)) & 1) + '0';
                            }

                            memcpy(buffer + 19, SM[i].send_buffer[SM[i].swnd.wndw[j]], len);
                            sendto(SM[i].udp_sockid, buffer, len + 19, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                            SM[i].last_sent_time[j] = time(NULL);
                            total_transmissions++;
                        }
                        j = (j + 1) % MAX_SEQNO;
                    }
                }
            }
        }
        V(sem_SM);
    }
}
