/*
=====================================
Assignment 5
Misbehaving Client (Sends GET_TASK and does nothing)
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
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

int sock;
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];

void handler(int signum)
{
    printf("\nC: closing misbehaving client...\n");
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

    printf("C: misbehaving client connected to server at %s:%d\n", SERVER_IP, PORT);

    printf("C: requesting a task...\n");
    send(sock, "GET_TASK\n", strlen("GET_TASK\n"), 0);

    printf("C: sent GET_TASK, now doing nothing...\n");

    // Just wait indefinitely without responding
    int x = 6;
    while (x--)
    {
        sleep(10);
    }

    return 0;
}