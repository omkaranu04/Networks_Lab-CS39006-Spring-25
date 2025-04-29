/*
==============================
Assignment 4 Final Submission
Name: Omkar Bhandare
Roll Number: 22CS30016
==============================
*/
#include "ksocket.h" // SENDER PROCESS

int main(int argc, char const *argv[])
{
    int port = 5000, dest = 5001;
    if (argc == 3)
    {
        port = atoi(argv[1]);
        dest = atoi(argv[2]);
    }
    else if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int num;
    printf("Enter the input file number: ");
    scanf("%d", &num);

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0)
    {
        perror("error in socket");
        return -1;
    }

    printf("Socket (sender) created with sockfd %d\n", sockfd);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (k_bind(sockfd, addr.sin_addr.s_addr, addr.sin_port, inet_addr("127.0.0.1"), htons(dest)) == -1)
    {
        perror("error in k_bind");
        return -1;
    }

    sleep(1);

    char filename[20], buff[MESSAGELEN];
    sprintf(filename, "input%d.txt", num);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    int i = 0;
    while (1)
    {
        for (i = 0; i < MESSAGELEN; i++)
        {
            char c = fgetc(fp);
            if (c == EOF)
            {
                buff[i] = '\0';
                break;
            }
            buff[i] = c;
        }
        while (k_sendto(sockfd, buff, MESSAGELEN, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1)
        {
            printf("Sender retrying in 3 seconds ...\n");
            sleep(3);
        }
        printf("Sent %d bytes of data\n", i);
        if (i < MESSAGELEN)
            break;
    }
    fclose(fp);
    sleep(100000);
    k_close(sockfd);
    return 0;
}
