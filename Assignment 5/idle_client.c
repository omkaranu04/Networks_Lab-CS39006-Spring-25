/*
=====================================
Assignment 5
Idle Client Implementation
Name: Omkar Bhandare
Roll number: 22CS30016
=====================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

int sock;
struct sockaddr_in server_addr;

void handler(int signum)
{
    printf("\nC: closing idle client...\n");
    close(sock);
    exit(0);
}

int main()
{
    signal(SIGINT, handler);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("C: idle client connected to server at %s:%d\n", SERVER_IP, PORT);
    printf("C: staying idle for 1 minute after that self exiting ...\n");

    int x = 6;
    while (x--)
    {
        sleep(10);
    }
    printf("C: idle Process Exiting...\n");

    return 0;
}
