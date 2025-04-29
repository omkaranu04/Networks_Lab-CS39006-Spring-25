// user1.c

#include "ksocket.h"

int main(int argc, char const *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s <src_ip> <src_port> <dest_ip> <dest_port>\n", argv[0]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[513];

    if ((sockfd = k_socket(AF_INET, SOCK_KTP, 0)) < 0)
    {
        perror("error in socket creation");
        return 1;
    }

    char src_ip[16], dest_ip[16];
    uint16_t src_port, dest_port;
    strcpy(src_ip, argv[1]);
    src_port = atoi(argv[2]);
    strcpy(dest_ip, argv[3]);
    dest_port = atoi(argv[4]);

    // Corrected k_bind call to match the function signature
    if (k_bind(src_ip, src_port, dest_ip, dest_port) < 0)
    {
        perror("error in binding");
        return 1;
    }

    int fd = open("input.txt", O_RDONLY);
    if (fd < 0)
    {
        perror("error in opening file");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    server_addr.sin_addr.s_addr = inet_addr(dest_ip);

    printf("Reading from file ... \n");
    int read_len;
    buffer[0] = '0';
    int seq = 1, no_of_msgs = 0;

    while ((read_len = read(fd, buffer + 1, 512)) > 0)
    {
        printf("Sending %d bytes of data\n", read_len);
        int send_len;
        while (1)
        {
            while ((send_len = k_sendto(sockfd, buffer, read_len + 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0 && errno == ENOSPACE)
            {
                printf("Buffer full, retrying...\n");
                sleep(1);
            }

            if (send_len >= 0)
                break;
            perror("error in sending data");
            return 1;
        }

        printf("Sent %d bytes of data, seq %d\n", send_len, seq);
        seq = (seq + 1) % MAX_SEQNO;
        no_of_msgs++;
    }

    // Send EOF marker
    buffer[0] = '1';
    int send_len;
    while (1)
    {
        while ((send_len = k_sendto(sockfd, buffer, 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0 && errno == ENOSPACE)
        {
            printf("Buffer full, retrying...\n");
            sleep(1);
        }

        if (send_len >= 0)
        {
            no_of_msgs++;
            printf("Sending EOF. Total messages sent: %d\n", no_of_msgs);
            break;
        }

        perror("error in sending EOF");
        return 1;
    }

    sleep(5);
    printf("File sent successfully\n");
    close(fd);
    if (k_close(sockfd) < 0)
    {
        perror("error in closing socket");
        return 1;
    }

    return 0;
}