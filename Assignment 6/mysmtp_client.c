/*
====================================
Assignment 6: SMTP Server
Name: Omkar Vijay Bhandare
Roll Number: 22CS30016
====================================
*/

/*
Implementation Checks:
    1. You cannot any command before sending HELO.
    2. The domain name should always match with the domain name in HELO.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

int main(int argc, char const *argv[])
{
    int port = 2525;
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }
    if (argc == 3)
    {
        port = atoi(argv[2]);
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to My_SMTP server.\n");

    while (1)
    {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        int bytes_sent = send(sock, buffer, strlen(buffer), 0);
        if (bytes_sent < 0)
        {
            printf("Error sending data\n");
            break;
        }

        if (strcmp(buffer, "QUIT") == 0)
        {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_recv = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_recv <= 0)
            {
                printf("CONNECTION CLOSED BY SERVER\n");
                break;
            }
            printf("%s\n", buffer);
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            printf("CONNECTION CLOSED BY SERVER\n");
            break;
        }
        printf("%s\n", buffer);

        if (strncmp(buffer, "Enter your message", 18) == 0)
        {
            while (1)
            {
                char line[BUFFER_SIZE] = {0};
                fgets(line, BUFFER_SIZE, stdin);

                int len = strlen(line);
                if (len > 0 && line[len - 1] == '\n')
                {
                    line[len - 1] = '\0';
                }

                if (strcmp(line, ".") == 0)
                {
                    bytes_sent = send(sock, ".\r\n", 3, 0);
                    if (bytes_sent < 0)
                    {
                        printf("Error sending end of message marker\n");
                        close(sock);
                        return -1;
                    }
                    break;
                }

                strcat(line, "\r\n");
                bytes_sent = send(sock, line, strlen(line), 0);
                if (bytes_sent < 0)
                {
                    printf("Error sending message line\n");
                    close(sock);
                    return -1;
                }
            }

            memset(buffer, 0, BUFFER_SIZE);
            n = recv(sock, buffer, BUFFER_SIZE, 0);
            if (n <= 0)
            {
                printf("Connection closed by server\n");
                break;
            }
            printf("%s", buffer);
        }
    }
    close(sock);
    return 0;
}