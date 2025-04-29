/*
=====================================
Assignment 5
Normal Client Implementation
Name: Omkar Bhandare
Roll number: 22CS30016
=====================================
*/
// ASSUME THAT EACH CLIENT REQUESTS ONLY FOR 5 TASKS
// ASSUME THAT ALL THE TASKS ARE MATHEMATICALLY COREECT, NO CHECKS HAVE BEEN IMPLEMENTED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

int sock;
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];

int eval_expr(const char *expr)
{
    int a, b;
    char op;
    sscanf(expr, "%d %c %d", &a, &op, &b);

    switch (op)
    {
    case '+':
        return a + b;
    case '-':
        return a - b;
    case '*':
        return a * b;
    case '/':
        return a / b;
    default:
        return 0;
    }
}

void handler(int signum)
{
    memset(buffer, 0, BUFFER_SIZE);
    strcpy(buffer, "exit");
    send(sock, buffer, strlen(buffer), 0);

    close(sock);
    exit(0);
}

void send_message(int sock, const char *message)
{
    char temp[BUFFER_SIZE];
    snprintf(temp, BUFFER_SIZE, "%s\n", message);
    send(sock, temp, strlen(temp), 0);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, handler);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation error");
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

    printf("C: connected to server at %s:%d\n", SERVER_IP, PORT);

    for (int i = 0; i < 20; i++)
    {
        printf("C: requesting task %d...\n", i + 1);

        send_message(sock, "GET_TASK");

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sock, buffer, BUFFER_SIZE, 0);

        if (n <= 0)
        {
            perror("recv failed / server closed connection");
            close(sock);
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\n")] = '\0';
        printf("C: received: %s\n", buffer);
        if (strncmp(buffer, "No tasks available", 18) == 0)
        {
            printf("C: no tasks available. Exiting...\n");
            break;
        }

        if (strncmp(buffer, "Task: ", 6) == 0)
        {
            char *task = buffer + 6;
            printf("C: processing task: %s\n", task);

            int result = eval_expr(task);
            printf("C: result calculated = %d\n", result);

            memset(buffer, 0, BUFFER_SIZE);
            snprintf(buffer, BUFFER_SIZE, "RESULT %d\n", result);
            send_message(sock, buffer);
            printf("C: sent result: %d\n", result);

            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE, 0);
            buffer[strcspn(buffer, "\n")] = '\0';
            if (strcmp(buffer, "Result received") != 0)
            {
                printf("Error: Unexpected response from server %s\n", buffer);
                break;
            }
            printf("\n");
        }
    }

    send_message(sock, "exit");
    close(sock);
    return 0;
}