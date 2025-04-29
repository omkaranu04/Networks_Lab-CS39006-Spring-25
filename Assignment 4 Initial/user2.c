// user2.c

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
    socklen_t addr_len = sizeof(server_addr);

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

    char filename[100];
    sprintf(filename, "output_%d.txt", dest_port);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        perror("error in opening file");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    server_addr.sin_addr.s_addr = inet_addr(dest_ip);

    printf("Receiving data ... \n");
    int recv_len;
    while (1)
    {
        while ((recv_len = k_recvfrom(sockfd, buffer, 513, 0, (struct sockaddr *)&server_addr, &addr_len)) <= 0)
        {
            printf("Checking the buffer\n");
            if (recv_len < 0 && errno == ENOMESSAGE)
            {
                sleep(1);
                continue;
            }
            if (recv_len < 0)
            {
                perror("error in receiving data");
                break;
            }
        }

        if (recv_len <= 0)
            break;

        if (buffer[0] == '1')
        {
            printf("Received the end of the file\n");
            break;
        }

        printf("Writing %d bytes of data\n", recv_len - 1);
        if (write(fd, buffer + 1, recv_len - 1) < 0)
        {
            perror("error in writing to file");
            return 1;
        }
    }

    close(fd);
    sleep(5);
    if (k_close(sockfd) < 0)
    {
        perror("error in closing socket");
        return 1;
    }

    return 0;
}