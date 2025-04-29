/*
=====================================
Assignment 5
Client Implementation
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

// Function to evaluate arithmetic expressions
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
        return (b != 0) ? a / b : 0;
    default:
        return 0;
    }
}

// Signal handler for CTRL+C
void handler(int signum)
{
    printf("\nC: closing client...\n");
    close(sock);
    exit(0);
}

// Function to send a message with newline delimiter
void send_message(const char *message)
{
    char temp[BUFFER_SIZE];
    snprintf(temp, BUFFER_SIZE, "%s\n", message);
    send(sock, temp, strlen(temp), 0);
}

// Function to create and connect socket
int connect_to_server()
{
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

    printf("C: connected to server at %s:%d\n", SERVER_IP, PORT);
    return sock;
}

// 1. Client that sends repeated GET_TASK without completing previous task
void repeat_client()
{
    connect_to_server();

    for (int i = 0; i < 3; i++)
    {
        printf("C: requesting task %d...\n", i + 1);
        send_message("GET_TASK");

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            perror("recv failed");
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
            printf("C: calculated result: %d\n", result);

            printf("C: requesting another task before sending the result!\n");
            // Instead of sending the result, request another task
            sleep(1);
        }
    }

    send_message("exit");
    close(sock);
    printf("C: repeat client exited\n");
}

// 2. Client that sends GET_TASK and then does not respond
void send_and_idle_client()
{
    connect_to_server();

    printf("C: requesting a task...\n");
    send_message("GET_TASK");

    memset(buffer, 0, BUFFER_SIZE);
    int n = recv(sock, buffer, BUFFER_SIZE, 0);
    if (n <= 0)
    {
        perror("recv failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("C: received: %s\n", buffer);

    printf("C: sent GET_TASK, now doing nothing...\n");
    // Just wait indefinitely without responding
    while (1)
    {
        sleep(10);
    }
}

// 3. Client that connects to server and does not respond further
void connect_and_idle_client()
{
    connect_to_server();

    printf("C: idle client connected to server, now doing nothing...\n");
    // Just wait indefinitely without sending any messages
    while (1)
    {
        sleep(10);
    }
}

// 4. Client that sends GET_TASK and then closes connection arbitrarily
void arbitrary_close_client()
{
    connect_to_server();

    printf("C: requesting a task...\n");
    send_message("GET_TASK");

    memset(buffer, 0, BUFFER_SIZE);
    int n = recv(sock, buffer, BUFFER_SIZE, 0);
    if (n <= 0)
    {
        perror("recv failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';
    printf("C: received: %s\n", buffer);

    if (strncmp(buffer, "Task: ", 6) == 0)
    {
        char *task = buffer + 6;
        printf("C: processing task: %s\n", task);
        int result = eval_expr(task);
        printf("C: calculated result: %d\n", result);

        printf("C: closing connection arbitrarily without sending result!\n");
        sleep(1);
        close(sock);
        printf("C: client exited without sending the solution!\n");
        exit(0);
    }
}

// 5. Normal client that behaves correctly
void normal_client()
{
    connect_to_server();

    for (int i = 0; i < 5; i++)
    {
        printf("C: requesting task %d...\n", i + 1);
        send_message("GET_TASK");

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

            char result_buffer[BUFFER_SIZE];
            snprintf(result_buffer, BUFFER_SIZE, "RESULT %d", result);
            send_message(result_buffer);
            printf("C: sent result: %d\n", result);

            memset(buffer, 0, BUFFER_SIZE);
            n = recv(sock, buffer, BUFFER_SIZE, 0);
            buffer[strcspn(buffer, "\n")] = '\0';

            if (strcmp(buffer, "Result received") != 0)
            {
                printf("Error: Unexpected response from server %s\n", buffer);
                break;
            }
        }

        printf("\n");
    }

    send_message("exit");
    close(sock);
    printf("C: normal client exited\n");
}

int main()
{
    signal(SIGINT, handler);

    int choice;
    printf("Choose client type:\n");
    printf("1. Send repeated GET_TASK without completing previous task\n");
    printf("2. Send GET_TASK and then do not respond\n");
    printf("3. Connect to server and do not respond further\n");
    printf("4. Send GET_TASK and then close connection arbitrarily\n");
    printf("5. Normal client behavior\n");
    printf("Enter choice (1-5): ");
    scanf("%d", &choice);

    switch (choice)
    {
    case 1:
        repeat_client();
        break;
    case 2:
        send_and_idle_client();
        break;
    case 3:
        connect_and_idle_client();
        break;
    case 4:
        arbitrary_close_client();
        break;
    case 5:
        normal_client();
        break;
    default:
        printf("Invalid choice!\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
