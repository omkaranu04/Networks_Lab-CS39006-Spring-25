/*
==============================
Assignment 4 Final Submission
Name: Omkar Bhandare
Roll Number: 22CS30016
==============================
*/
#include "ksocket.h"

int drop_message()
{
    return (rand() % 100) < p * 100;
}

ssize_t createACKpacket(char *buf, int n, int seq, int ack, int recv_wndw)
{
    char header[HEADERSIZE];
    if (n != HEADERSIZE)
        return -1;

    header[0] = (char)(seq & 0xFF);
    header[1] = '1'; // ACK packet identifier
    header[2] = (char)(ack & 0xFF);
    header[3] = (char)(recv_wndw & 0xFF);

    for (int i = 0; i < HEADERSIZE; i++)
    {
        buf[i] = header[i];
    }
    return n;
}
ssize_t createDATApacket(char *buf, int n, char *data, int seq, int ack, int recv_wndw)
{
    char header[HEADERSIZE];
    if (n != HEADERSIZE + MESSAGELEN)
        return -1;

    header[0] = (char)(seq & 0xFF);
    header[1] = '0'; // DATA packet identifier
    header[2] = (char)(ack & 0xFF);
    header[3] = (char)(recv_wndw & 0xFF);

    for (int i = 0; i < HEADERSIZE; i++)
    {
        buf[i] = header[i];
    }
    for (int i = 0; i < MESSAGELEN; i++)
    {
        buf[i + HEADERSIZE] = data[i];
    }
    return n;
}
ssize_t open_packet(char *buf, int n, int *seq, int *ack, int *is_ack, int *recv_wndw, char *data)
{
    char header[HEADERSIZE];
    int i = 0;

    do
    {
        header[i] = buf[i];
        i++;
    } while (i < HEADERSIZE);

    *seq = (unsigned char)header[0];
    *is_ack = (header[1] == '1') ? 1 : 0;
    *ack = (unsigned char)header[2];
    *recv_wndw = (unsigned char)header[3];

    // extract data if not ACK
    if (!*is_ack)
    {
        i = 0;
        do
        {
            data[i] = buf[i + HEADERSIZE];
            i++;
        } while (i < MESSAGELEN);
    }

    // adjust the sequence number
    *seq = (*seq - 1 + SEQNUM) % SEQNUM; // starts from 1
    return n - HEADERSIZE;
}

int inwndw(int seq, int head, int tail)
{
    if (seq < 0 || seq >= SEQNUM)
        return 0;
    if (head == tail)
        return 0;
    if (head < tail)
    { // normal window
        if (seq >= head && seq < tail)
            return 1;
        return 0;
    }
    else
    { // wrapped around window
        if (seq >= head || seq < tail)
            return 1;
        return 0;
    }
}
int wndwsize(struct window *wndw) // returns the window size, mostly for piggyback ACKs
{
    int temp = wndw->tail - wndw->head;
    if (temp < 0)
        temp += SEQNUM;
    return temp;
}

void check_and_send_window_updates(SMData *shared_memory, sem_t *socket_locks[], time_t last_update_times[])
{
    int socket_index;
    for (socket_index = 0; socket_index < MAXSOCKETS; socket_index++)
    {
        if (!shared_memory->sockets[socket_index].in_use)
            continue;

        sem_wait(socket_locks[socket_index]);

        if (shared_memory->sockets[socket_index].nospace)
        {
            int current_window_size = wndwsize(&(shared_memory->sockets[socket_index].rwnd));
            if (current_window_size > 0)
            {
                printf("R: Sending ACK for no space in socket %d\n", socket_index);

                // Create and send window update packet
                int sequence_to_ack = shared_memory->sockets[socket_index].last_acked;
                char packet_buffer[HEADERSIZE];
                ssize_t packet_size = createACKpacket(
                    packet_buffer,
                    HEADERSIZE,
                    0,
                    sequence_to_ack,
                    current_window_size);

                // Setup destination address
                struct sockaddr_in destination;
                destination.sin_family = AF_INET;
                destination.sin_port = shared_memory->sockets[socket_index].dest.port;
                destination.sin_addr.s_addr = shared_memory->sockets[socket_index].dest.ip;

                // Send the packet
                sendto(
                    shared_memory->sockets[socket_index].sockfd,
                    packet_buffer,
                    HEADERSIZE,
                    0,
                    (struct sockaddr *)&destination,
                    sizeof(destination));

                // Update the timestamp
                last_update_times[socket_index] = time(NULL);
            }
        }

        sem_post(socket_locks[socket_index]);
    }
}

void handle_socket_data(SMData *shared_memory, sem_t *socket_locks[], fd_set *ready_sockets)
{
    int socket_index;
    for (socket_index = 0; socket_index < MAXSOCKETS; socket_index++)
    {
        int current_fd = shared_memory->sockets[socket_index].sockfd;

        if (!FD_ISSET(current_fd, ready_sockets))
            continue;

        printf("R: Message received on socket %d\n", current_fd);
        sem_wait(socket_locks[socket_index]);

        // Skip processing if socket was closed
        if (!shared_memory->sockets[socket_index].in_use)
        {
            sem_post(socket_locks[socket_index]);
            continue;
        }

        // Prepare to receive data
        char receive_buffer[HEADERSIZE + MESSAGELEN];
        struct sockaddr_in sender_address;
        socklen_t address_length = sizeof(sender_address);

        // Receive data from socket
        int bytes_received = recvfrom(
            current_fd,
            receive_buffer,
            HEADERSIZE + MESSAGELEN,
            0,
            (struct sockaddr *)&sender_address,
            &address_length);

        // Handle receive errors
        if (bytes_received == -1)
        {
            perror("error in recvfrom");
            exit(1);
        }

        // Handle connection closure
        if (bytes_received == 0)
        {
            close(current_fd);
            shared_memory->sockets[socket_index].in_use = 0;
            shared_memory->sockets[socket_index].pid = 0;
            sem_post(socket_locks[socket_index]);
            continue;
        }

        // Verify packet source
        if (sender_address.sin_addr.s_addr != shared_memory->sockets[socket_index].dest.ip ||
            sender_address.sin_port != shared_memory->sockets[socket_index].dest.port)
        {
            sem_post(socket_locks[socket_index]);
            continue;
        }

        // ----- Begin former helper function logic -----
        ktp_socket *active_socket = &(shared_memory->sockets[socket_index]);

        // Parse packet
        int seq_number, ack_number, is_ack_packet, receiver_window;
        char message_data[MESSAGELEN];
        int data_size = open_packet(
            receive_buffer,
            bytes_received,
            &seq_number,
            &ack_number,
            &is_ack_packet,
            &receiver_window,
            message_data);

        printf("SEQ: %d, ACK: %d, is_ACK: %d, RWND: %d\n",
               seq_number + 1, ack_number, is_ack_packet, receiver_window);

        // Simulate packet loss
        if (drop_message())
        {
            printf("--- Packet dropped with seq no. = %d\n", seq_number + 1);
            sem_post(socket_locks[socket_index]);
            continue;
        }

        if (!is_ack_packet)
        {
            // Handle DATA packet
            if (inwndw(seq_number, active_socket->rwnd.head, active_socket->rwnd.tail))
            {
                // Received packet is within the window
                active_socket->nospace = 0;
                printf("R: Packet within window size\n");

                // Calculate buffer index from sequence number
                int buffer_idx = seq_number % MAXMESSAGE;

                if (active_socket->recv_buffer.isempty[buffer_idx])
                {
                    printf("R: Buffer is empty, writing in buffer indexed %d\n", buffer_idx);

                    // Copy received data to buffer
                    for (int i = 0; i < MESSAGELEN; i++)
                    {
                        active_socket->recv_buffer.buff[buffer_idx][i] = message_data[i];
                    }

                    active_socket->recv_buffer.isempty[buffer_idx] = 0;
                    active_socket->recv_buffer.filled++;

                    // If this was the expected packet, move the window ahead
                    if (seq_number == active_socket->rwnd.head)
                    {
                        int ack_value = active_socket->rwnd.head;

                        // Find highest possible ACK number for cumulative ACK
                        while (inwndw(ack_value, active_socket->rwnd.head, active_socket->rwnd.tail) &&
                               !active_socket->recv_buffer.isempty[ack_value % MAXMESSAGE])
                        {
                            ack_value = (ack_value + 1) % SEQNUM;
                        }

                        ack_value = (ack_value - 1 + SEQNUM) % SEQNUM;
                        active_socket->rwnd.head = (ack_value + 1) % SEQNUM;

                        // Send ACK packet with window size update
                        char ack_packet[HEADERSIZE];
                        int window_size = wndwsize(&(active_socket->rwnd));

                        ssize_t packet_size = createACKpacket(
                            ack_packet,
                            HEADERSIZE,
                            0,
                            ack_value,
                            window_size);

                        active_socket->last_acked = ack_value;

                        sendto(
                            active_socket->sockfd,
                            ack_packet,
                            HEADERSIZE,
                            0,
                            (struct sockaddr *)&sender_address,
                            sizeof(sender_address));

                        printf("R: Sending ACK%d RWND-SIZE = %d Packet-Size = %ld\n",
                               ack_value + 1, window_size, packet_size);

                        if (window_size == 0)
                        {
                            printf("R: RWND-SIZE = 0, updating the nospace status to 1\n");
                            active_socket->nospace = 1;
                        }
                    }
                }
            }
            else if (wndwsize(&(active_socket->rwnd)) == 0)
            {
                // Receive window size is zero
                printf("R: RWND-SIZE = 0, updating the nospace status to 1\n");
                active_socket->nospace = 1;

                int last_ack = active_socket->last_acked;
                char zero_window_ack[HEADERSIZE];

                // Send ACK to indicate zero window size
                ssize_t packet_size = createACKpacket(
                    zero_window_ack,
                    HEADERSIZE,
                    0,
                    last_ack,
                    0);

                sendto(
                    active_socket->sockfd,
                    zero_window_ack,
                    HEADERSIZE,
                    0,
                    (struct sockaddr *)&sender_address,
                    sizeof(sender_address));

                printf("R: Sending ACK%d RWND-SIZE = 0 Packet-Size = %ld\n",
                       last_ack, packet_size);
            }
            else if ((seq_number + 1) % SEQNUM == active_socket->rwnd.head)
            {
                // Duplicate packet of the immediate ACKed one
                char duplicate_ack[HEADERSIZE];

                ssize_t packet_size = createACKpacket(
                    duplicate_ack,
                    HEADERSIZE,
                    seq_number,
                    seq_number,
                    wndwsize(&(active_socket->rwnd)));

                sendto(
                    active_socket->sockfd,
                    duplicate_ack,
                    HEADERSIZE,
                    0,
                    (struct sockaddr *)&sender_address,
                    sizeof(sender_address));
            }
        }
        else
        {
            // Handle ACK packet
            if (wndwsize(&(active_socket->swnd)) == 0)
            {
                // Sender window is full
                int last_sent = (active_socket->swnd.tail - 1 + SEQNUM) % SEQNUM;

                if (ack_number == last_sent)
                {
                    printf("R: Space found in SWND\n");
                    active_socket->swnd.size = receiver_window;
                }
            }

            // Check if ACK is in sender window
            if (!inwndw(ack_number, active_socket->swnd.head, active_socket->swnd.tail))
            {
                printf("R: ACK received not in SWND\n");
                sem_post(socket_locks[socket_index]);
                continue;
            }

            // Process ACKs and update window
            while (1)
            {
                int buffer_index = active_socket->swnd.head % MAXMESSAGE;

                if (!active_socket->send_buffer.isempty[buffer_index])
                {
                    // Mark buffer as empty
                    active_socket->send_buffer.isempty[buffer_index] = 1;
                    memset(active_socket->send_buffer.buff[buffer_index], 0, MESSAGELEN);
                    active_socket->send_buffer.filled--;

                    // Move window head
                    active_socket->swnd.head = (active_socket->swnd.head + 1) % SEQNUM;
                }

                // Break if reached expected ACK
                if ((ack_number + 1) % SEQNUM == active_socket->swnd.head)
                {
                    break;
                }
            }

            // Update window size
            active_socket->swnd.size = receiver_window;
        }
        // ----- End former helper function logic -----

        sem_post(socket_locks[socket_index]);
    }
}

void *RThread()
{
    // Initialize timestamp tracking array
    time_t update_timestamps[MAXSOCKETS];
    int i;
    for (i = 0; i < MAXSOCKETS; i++)
    {
        update_timestamps[i] = 0;
    }

    // Get shared memory reference
    SMData *shared_memory = get_shm();

    // Initialize file descriptor set for select()
    fd_set active_sockets;
    FD_ZERO(&active_sockets);
    int highest_fd = 0;

    // Initialize mutex locks for each socket
    sem_t *socket_locks[MAXSOCKETS];
    for (i = 0; i < MAXSOCKETS; i++)
    {
        socket_locks[i] = sem_open(shared_memory->sockets[i].socket_mutex, O_CREAT, 0666, 1);
    }

    // Main processing loop
    while (1)
    {
        // Reset file descriptor set
        FD_ZERO(&active_sockets);
        highest_fd = 0;

        // Add active sockets to monitoring set
        for (i = 0; i < MAXSOCKETS; i++)
        {
            if (shared_memory->sockets[i].in_use)
            {
                sem_wait(socket_locks[i]);
                int sock = shared_memory->sockets[i].sockfd;
                FD_SET(sock, &active_sockets);
                sem_post(socket_locks[i]);

                if (sock > highest_fd)
                {
                    highest_fd = sock;
                }
            }
        }

        // Setup select timeout
        struct timeval timeout = {T, 0};
        int select_result = select(highest_fd + 1, &active_sockets, NULL, NULL, &timeout);
        sleep(1);

        // Handle select errors
        if (select_result == -1)
        {
            perror("error in select");
            exit(1);
        }

        // Handle timeout case - send window updates if needed
        if (select_result == 0)
        {
            check_and_send_window_updates(shared_memory, socket_locks, update_timestamps);
            continue;
        }

        // Perform periodic window update check
        time_t current_time = time(NULL);
        for (i = 0; i < MAXSOCKETS; i++)
        {
            if (!shared_memory->sockets[i].in_use)
                continue;

            sem_wait(socket_locks[i]);

            if (shared_memory->sockets[i].nospace && current_time >= (update_timestamps[i] + T))
            {
                int window_size = wndwsize(&(shared_memory->sockets[i].rwnd));
                if (window_size > 0)
                {
                    printf("R: Sending ACK for no space in socket %d\n", i);

                    // Create ACK packet with updated window size
                    int ack_number = shared_memory->sockets[i].last_acked;
                    char ack_buffer[HEADERSIZE];
                    ssize_t packet_length = createACKpacket(
                        ack_buffer,
                        HEADERSIZE,
                        0,
                        ack_number,
                        window_size);

                    // Setup destination address
                    struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = shared_memory->sockets[i].dest.port;
                    dest_addr.sin_addr.s_addr = shared_memory->sockets[i].dest.ip;

                    // Send the packet
                    sendto(
                        shared_memory->sockets[i].sockfd,
                        ack_buffer,
                        HEADERSIZE,
                        0,
                        (struct sockaddr *)&dest_addr,
                        sizeof(dest_addr));

                    // Update timestamp
                    update_timestamps[i] = current_time;
                }
            }

            sem_post(socket_locks[i]);
        }

        // Process data on ready sockets
        handle_socket_data(shared_memory, socket_locks, &active_sockets);
    }
}

void transmit_segment(ktp_socket *socket, int begin_seq, int end_seq)
{
    int current_seq = begin_seq;

    while (current_seq != end_seq)
    {
        // Calculate buffer position for current sequence
        int buffer_index = current_seq % MAXMESSAGE;

        // Prepare packet buffer
        char packet_buffer[HEADERSIZE + MESSAGELEN];
        memset(packet_buffer, 0, HEADERSIZE + MESSAGELEN);

        // Create data packet
        int packet_size = createDATApacket(
            packet_buffer,
            HEADERSIZE + MESSAGELEN,
            socket->send_buffer.buff[buffer_index],
            current_seq,
            0,
            0);

        // Set up destination address
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = socket->dest.port;
        dest_addr.sin_addr.s_addr = socket->dest.ip;

        // Send the packet
        printf("S: Sent a packet with seq num %d\n", current_seq + 1);
        int bytes_sent = sendto(
            socket->sockfd,
            packet_buffer,
            packet_size,
            0,
            (struct sockaddr *)&dest_addr,
            sizeof(dest_addr));

        // Move to next sequence number
        current_seq = (current_seq + 1) % SEQNUM;
    }
}

void check_retransmission(
    time_t last_transmission_times[MAXSOCKETS][2],
    int window_heads[MAXSOCKETS][2],
    int window_tails[MAXSOCKETS][2],
    int socket_index,
    time_t current_time,
    int *needs_transmission,
    ktp_socket *socket)
{
    // Check if first transmission record exists
    if (last_transmission_times[socket_index][0] != -1)
    {
        // Calculate the last sent sequence number
        int last_sent = (window_tails[socket_index][0] - 1 + SEQNUM) % SEQNUM;

        // Check if last sent sequence is still in the window
        if (inwndw(last_sent, socket->swnd.head, socket->swnd.tail))
        {
            // Check if timeout period has elapsed
            if (current_time >= last_transmission_times[socket_index][0] + T)
            {
                // If second transmission exists, merge it with first
                if (last_transmission_times[socket_index][1] != -1)
                {
                    last_transmission_times[socket_index][1] = -1;
                    window_heads[socket_index][1] = -1;
                    window_tails[socket_index][0] = window_tails[socket_index][1];
                    window_tails[socket_index][1] = -1;
                }

                // Update timestamp and mark for retransmission
                last_transmission_times[socket_index][0] = current_time;
                *needs_transmission = 1;
            }
        }
        else
        {
            // Shift second record to first if last sent is no longer in window
            last_transmission_times[socket_index][0] = last_transmission_times[socket_index][1];
            window_heads[socket_index][0] = window_heads[socket_index][1];
            window_tails[socket_index][0] = window_tails[socket_index][1];

            // Clear second record
            last_transmission_times[socket_index][1] = -1;
            window_heads[socket_index][1] = -1;
            window_tails[socket_index][1] = -1;
        }
    }
}

void *SThread()
{
    // Get shared memory reference
    SMData *shared_mem = get_shm();

    // Initialize mutex locks
    sem_t *socket_locks[MAXSOCKETS];
    int i = 0;
    do
    {
        socket_locks[i] = sem_open(shared_mem->sockets[i].socket_mutex, O_CREAT, 0666, 1);
        i++;
    } while (i < MAXSOCKETS);

    // Initialize transmission tracking arrays
    time_t last_transmission_times[MAXSOCKETS][2]; // Replaces timeout.time
    int window_heads[MAXSOCKETS][2];               // Replaces timeout.head
    int window_tails[MAXSOCKETS][2];               // Replaces timeout.tail

    // Initialize tracking arrays
    i = 0;
    do
    {
        int j = 0;
        do
        {
            last_transmission_times[i][j] = -1;
            window_heads[i][j] = -1;
            window_tails[i][j] = -1;
            j++;
        } while (j < 2);
        i++;
    } while (i < MAXSOCKETS);

    // Main processing loop
    while (1)
    {
        sleep(T / 2);
        time_t current_time = time(NULL);

        // Process each socket
        int socket_index = 0;
        do
        {
            sem_wait(socket_locks[socket_index]);

            ktp_socket *current_socket = &(shared_mem->sockets[socket_index]);
            if (current_socket->in_use)
            {
                int transmission_needed = 0;

                // Check for timeouts and retransmission
                check_retransmission(
                    last_transmission_times,
                    window_heads,
                    window_tails,
                    socket_index,
                    current_time,
                    &transmission_needed,
                    current_socket);

                // Check for new data to send
                int current_seq = current_socket->swnd.tail;
                do
                {
                    if (current_socket->send_buffer.isempty[current_seq % MAXMESSAGE] ||
                        wndwsize(&(current_socket->swnd)) >= current_socket->swnd.size)
                    {
                        break;
                    }

                    current_socket->swnd.tail = (current_socket->swnd.tail + 1) % SEQNUM;
                    current_seq = (current_seq + 1) % SEQNUM;
                    transmission_needed = 1;
                } while (1);

                // Skip if no transmission needed
                if (!transmission_needed)
                {
                    sem_post(socket_locks[socket_index]);
                    socket_index++;
                    continue;
                }

                // Handle transmission scheduling
                if (last_transmission_times[socket_index][0] == -1 ||
                    last_transmission_times[socket_index][0] == current_time)
                {
                    // First transmission or retransmission
                    last_transmission_times[socket_index][0] = current_time;
                    window_heads[socket_index][0] = current_socket->swnd.head;
                    window_tails[socket_index][0] = current_socket->swnd.tail;

                    transmit_segment(
                        current_socket,
                        window_heads[socket_index][0],
                        window_tails[socket_index][0]);
                }
                else
                {
                    // New segment transmission
                    last_transmission_times[socket_index][1] = current_time;
                    window_heads[socket_index][1] = window_tails[socket_index][0];
                    window_tails[socket_index][1] = current_socket->swnd.tail;

                    transmit_segment(
                        current_socket,
                        window_heads[socket_index][1],
                        window_tails[socket_index][1]);
                }
            }

            sem_post(socket_locks[socket_index]);
            socket_index++;
        } while (socket_index < MAXSOCKETS);
    }
}

void *GCThread()
{
    SMData *shm = get_shm();
    while (1)
    {
        sleep(GCTIMEOUT);

        int i = 0;
        do
        {
            if (shm->sockets[i].in_use)
            {
                if (kill(shm->sockets[i].pid, 0) == -1)
                {
                    printf("GC: Process owning socket %d is dead. Closing the socket ...\n", i);
                    shm->sockets[i].is_closed = 1;
                }
            }
            i++;
        } while (i < MAXSOCKETS);
    }
}

int bind_if_required(int socket_index, ktp_socket *socket_ptr)
{
    struct sockaddr_in current_addr;
    socklen_t addr_len = sizeof(current_addr);

    if (getsockname(socket_ptr->sockfd, (struct sockaddr *)&current_addr, &addr_len) == -1)
    {
        perror("getsockname failed");
        return -1;
    }

    if ((current_addr.sin_port != socket_ptr->src.port ||
         current_addr.sin_addr.s_addr != socket_ptr->src.ip) &&
        socket_ptr->is_bound) // if socket is bound to wrong address
    {
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = socket_ptr->src.port;
        bind_addr.sin_addr.s_addr = socket_ptr->src.ip;

        printf("Bound Socket %d to %s : %d\n", socket_index,
               inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port));

        if (bind(socket_ptr->sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1)
        {
            perror("error in bind");
            return -1;
        }
    }

    return 0;
}
void handler(int signum);

int main(int argc, char const *argv[])
{
    // server init
    // create and attach shared memory
    key_t key = ftok(".", 'A');
    int shmid = shmget(key, sizeof(SMData), IPC_CREAT | 0666 | IPC_EXCL);
    if (shmid < 0)
    {
        perror("error shmget");
        exit(1);
    }
    SMData *shm = (SMData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1)
    {
        perror("error shmat");
        exit(1);
    }

    int i = 0;
    do
    {
        sockreset(&(shm->sockets[i]));
        shm->sockets[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (shm->sockets[i].sockfd == -1)
        {
            perror("error in socket");
            exit(1);
        }
        sprintf(shm->sockets[i].socket_mutex, "mutex%d", i);
        sem_t *mutex = sem_open(shm->sockets[i].socket_mutex, O_CREAT, 0666, 1);
        sem_close(mutex);
        i++;
    } while (i < MAXSOCKETS);

    // threads
    pthread_t r_thread, s_thread, gc_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&r_thread, &attr, RThread, NULL);
    pthread_create(&s_thread, &attr, SThread, NULL);
    pthread_create(&gc_thread, &attr, GCThread, NULL);

    shm = get_shm();

    signal(SIGINT, handler);
    signal(SIGSEGV, handler);

    while (1)
    {
        sleep(1);

        i = 0;
        do
        {
            sem_t *mutex = sem_open(shm->sockets[i].socket_mutex, O_CREAT, 0666, 1);
            sem_wait(mutex);

            // skip not in use sockets
            if (shm->sockets[i].in_use == 0)
            {
                sem_post(mutex);
                i++;
                continue;
            }

            // clean terminated sockets
            if (shm->sockets[i].is_closed)
            {
                printf("Found a terminated socket %d, resetting ...\n", i);
                sockreset(&(shm->sockets[i]));
                sem_post(mutex);
                i++;
                continue;
            }

            if (shm->sockets[i].sockfd == -1) // initialise new sockets, if needed
            {
                shm->sockets[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                if (shm->sockets[i].sockfd == -1)
                {
                    perror("error in socket creation");
                    exit(1);
                }
            }

            if (bind_if_required(i, &(shm->sockets[i])) == -1)
            {
                exit(1);
            }

            sem_post(mutex);
            i++;
        } while (i < MAXSOCKETS);
    }
    return 0;
}

void handler(int signum)
{
    key_t key = ftok(".", 'A');
    int shmid = shmget(key, sizeof(SMData), 0666);
    if (shmid < 0)
    {
        perror("error shmget");
        exit(1);
    }

    SMData *shm = (SMData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1)
    {
        perror("error shmat");
        exit(1);
    }

    int i = 0;
    do
    {
        if (shm->sockets[i].in_use)
        {
            kill(shm->sockets[i].pid, SIGINT);
            close(shm->sockets[i].sockfd);
            shm->sockets[i].in_use = 0;
            shm->sockets[i].pid = 0;
            shm->sockets[i].sockfd = 0;

            sem_unlink(shm->sockets[i].socket_mutex);
        }
        i++;
    } while (i < MAXSOCKETS);

    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}
