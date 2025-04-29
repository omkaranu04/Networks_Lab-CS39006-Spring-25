/*
==============================
Assignment 4 Final Submission
Name: Omkar Bhandare
Roll Number: 22CS30016
==============================
*/
#include "ksocket.h" // RECEIVER PROCESS

int main(int argc, char const *argv[])
{
    int port = 5001, source = 5000; // defaulted ports
    if (argc == 3)
    {
        port = atoi(argv[1]);
        source = atoi(argv[2]);
    }
    else if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0); // create socket
    if (sockfd < 0)
    {
        perror("error in socket");
        return -1;
    }

    printf("Socket (receiver) created with sockfd %d\n", sockfd);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (k_bind(sockfd, addr.sin_addr.s_addr, addr.sin_port, inet_addr("127.0.0.1"), htons(source)) < 0) // bind socket
    {
        perror("error in k_bind");
        return -1;
    }

    sleep(5);

    char buff[MESSAGELEN];
    struct sockaddr_in dest_addr;
    socklen_t addr_len = sizeof(dest_addr);

    char filename[256];
    snprintf(filename, sizeof(filename), "output%d.txt", port);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        perror("Error opening output file");
        k_close(sockfd);
        return -1;
    }

    printf("----- Receiving data, will save to %s\n", filename);

    while (1)
    {
        int n = k_recvfrom(sockfd, buff, MESSAGELEN, 0, (struct sockaddr *)&dest_addr, &addr_len);
        if (n == -1)
        {
            printf("Retry Receiving after 3 seconds ... \n");
            sleep(3);
            continue;
        }
        int i = 0, temp = 0;
        for (i = 0; i < n; i++)
        {
            if (buff[i] == '\0')
            {
                temp = 1;
                break;
            }
        }
        printf("received %d bytes of data\n", i);
        fwrite(buff, sizeof(char), i, fp);
        fflush(fp);
        if (temp)
            break;
    }
    fclose(fp);
    k_close(sockfd);
    return 0;
}