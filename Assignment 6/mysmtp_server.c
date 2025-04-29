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
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define MAILBOX_DIR "mailbox"

int check_valid_email(const char *email)
{
    const char *at_sign = strchr(email, '@');
    if (!at_sign)
        return 0;

    if (at_sign == email || *(at_sign + 1) == '\0')
        return 0;

    const char *dot = strchr(at_sign, '.');
    if (!dot || *(dot + 1) == '\0')
        return 0;

    return 1;
}

void extract_domain(const char *input, char *domain, size_t domain_size)
{
    const char *at_sign = strchr(input, '@');
    if (at_sign)
    {
        strncpy(domain, at_sign + 1, domain_size - 1);
        domain[domain_size - 1] = '\0';
    }
    else
    {
        const char *space = strchr(input, ' ');
        if (space && *(space + 1) != '\0')
        {
            strncpy(domain, space + 1, domain_size - 1);
            domain[domain_size - 1] = '\0';
        }
        else
        {
            domain[0] = '\0';
        }
    }
}

void store_email(const char *recipient, const char *sender, const char *message)
{
    char filename[BUFFER_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.txt", MAILBOX_DIR, recipient);

    FILE *file = fopen(filename, "a");
    if (file == NULL)
    {
        perror("Failed to open mailbox file");
        return;
    }

    time_t now = time(NULL);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';

    fprintf(file, "From: %s\nDate: %s\n%s\n.\n", sender, date, message);
    fclose(file);
}

void list_emails(int client_socket, const char *recipient)
{
    char filename[BUFFER_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.txt", MAILBOX_DIR, recipient);

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        send(client_socket, "401 REQUESTED EMAIL NOT FOUND (EMAILS)\n", 40, 0);
        return;
    }

    char line[BUFFER_SIZE];
    int email_id = 1;
    char response[BUFFER_SIZE * 10] = "200 OK\n";

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "From:", 5) == 0)
        {
            char sender[BUFFER_SIZE];
            sscanf(line, "From: %s", sender);
            char date[BUFFER_SIZE];
            fgets(date, sizeof(date), file);
            char email_info[BUFFER_SIZE];
            snprintf(email_info, sizeof(email_info), "%d: Email from %.50s -- %.50s", email_id, sender, date + 6);

            if (strlen(response) + strlen(email_info) < BUFFER_SIZE * 10 - 1)
            {
                strcat(response, email_info);
            }
            else
            {
                break;
            }
            email_id++;
        }
    }

    fclose(file);
    send(client_socket, response, strlen(response), 0);
}

void get_email(int client_socket, const char *recipient, int id)
{
    char filename[BUFFER_SIZE];
    snprintf(filename, sizeof(filename), "%s/%s.txt", MAILBOX_DIR, recipient);

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        send(client_socket, "401 REQUESTED EMAIL NOT FOUND (EMAIL)\n", 39, 0);
        return;
    }

    char line[BUFFER_SIZE];
    int current_id = 0;
    char content[BUFFER_SIZE * 10];
    memset(content, 0, BUFFER_SIZE * 10);
    int found = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "From:", 5) == 0)
        {
            current_id++;
            if (current_id == id)
            {
                found = 1;
                strcat(content, line);
                while (fgets(line, sizeof(line), file))
                {
                    if (strncmp(line, ".", 1) == 0)
                        break;

                    if (strlen(content) + strlen(line) < BUFFER_SIZE * 10 - 1)
                    {
                        strcat(content, line);
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
        }
    }

    fclose(file);
    if (found)
        send(client_socket, content, strlen(content), 0);
    else
        send(client_socket, "401 REQUESTED EMAIL NOT FOUND (INDEX)\n", 38, 0);
}

void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());

    char buffer[BUFFER_SIZE], helo_domain[BUFFER_SIZE], sender[BUFFER_SIZE], recipient[BUFFER_SIZE], message[BUFFER_SIZE * 10];
    memset(buffer, 0, BUFFER_SIZE);
    memset(sender, 0, BUFFER_SIZE);
    memset(helo_domain, 0, BUFFER_SIZE);
    memset(recipient, 0, BUFFER_SIZE);
    memset(message, 0, BUFFER_SIZE * 10);

    int state = 0; // 0: HELO, 1: MAIL FROM, 2: RCPT TO, 3: DATA
    int helo_received = 0;

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
            break;

        buffer[n] = '\0';
        printf("Received: %s\n", buffer);

        if (strncmp(buffer, "QUIT", 4) == 0)
        {
            send(client_socket, "200 OK. Goodbye\n", 16, 0);
            break;
        }
        else if (strncmp(buffer, "HELO", 4) == 0 && state == 0)
        {
            extract_domain(buffer, helo_domain, sizeof(helo_domain));
            if (strlen(helo_domain) == 0)
            {
                send(client_socket, "400 ERR. INVALID DOMAIN\n", 25, 0);
            }
            else
            {
                send(client_socket, "200 OK. HELO ACCEPTED\n", 22, 0);
                state = 1;
                helo_received = 1;
                printf("HELO received from %s\n", helo_domain);
            }
        }
        else if (strncmp(buffer, "MAIL FROM:", 10) == 0 && state == 1)
        {
            sscanf(buffer, "MAIL FROM: %s", sender);
            if (!check_valid_email(sender))
            {
                send(client_socket, "400 ERR. INVALID EMAIL FORMAT\n", 29, 0);
                continue;
            }

            char sender_domain[BUFFER_SIZE];
            memset(sender_domain, 0, BUFFER_SIZE);
            extract_domain(sender, sender_domain, sizeof(sender_domain));

            if (strcmp(sender_domain, helo_domain) != 0)
            {
                send(client_socket, "403 FORBIDDEN: SENDER DOMAIN DOES NOT MATCH HELO DOMAIN\n", 56, 0);
                continue;
            }

            send(client_socket, "200 OK. RECEIVED SENDER EMAIL\n", 30, 0);
            state = 2;
            printf("MAIL FROM: %s\n", sender);
        }
        else if (strncmp(buffer, "RCPT TO:", 8) == 0 && state == 2)
        {
            sscanf(buffer, "RCPT TO: %s", recipient);

            if (!check_valid_email(recipient))
            {
                send(client_socket, "400 ERR: INVALID EMAIL FORMAT\n", 29, 0);
                continue;
            }

            send(client_socket, "200 OK. RECEIVED RECIPIENT EMAIL\n", 33, 0);
            state = 3;
            printf("RCPT TO: %s\n", recipient);
        }
        else if (strncmp(buffer, "DATA", 4) == 0 && state == 3)
        {
            send(client_socket, "Enter your message (end with a single dot '.'):\n", 49, 0);
            memset(message, 0, BUFFER_SIZE * 10);
            while (1)
            {
                memset(buffer, 0, BUFFER_SIZE);
                n = recv(client_socket, buffer, BUFFER_SIZE, 0);
                if (n <= 0)
                    break;

                if (strcmp(buffer, ".\r\n") == 0)
                    break;

                if (strlen(message) + strlen(buffer) < BUFFER_SIZE * 10 - 1)
                {
                    strcat(message, buffer);
                }
                else
                {
                    break;
                }
            }
            store_email(recipient, sender, message);
            send(client_socket, "200 OK. RECEIVED MAIL BODY\n", 27, 0);
            state = 1;
            printf("DATA RECEIVED, MESSAGE STORED.\n");
        }
        else if (strncmp(buffer, "LIST", 4) == 0)
        {
            if (!helo_received)
            {
                send(client_socket, "403 FORBIDDEN. MUST SEND HELO FIRST\n", 36, 0);
                continue;
            }

            char list_recipient[BUFFER_SIZE];
            sscanf(buffer, "LIST %s", list_recipient);

            if (!check_valid_email(list_recipient))
            {
                send(client_socket, "400 ERR. INVALID EMAIL FORMAT\n", 29, 0);
                continue;
            }

            char list_domain[BUFFER_SIZE];
            memset(list_domain, 0, BUFFER_SIZE);
            extract_domain(list_recipient, list_domain, sizeof(list_domain));

            if (strcmp(list_domain, helo_domain) != 0)
            {
                send(client_socket, "403 FORBIDDEN. LIST DOMAIN MUST MATCH HELO DOMAIN\n", 49, 0);
                continue;
            }

            list_emails(client_socket, list_recipient);
            printf("LIST %s\n", list_recipient);
            printf("EMAIL LIST SENT.\n");
        }
        else if (strncmp(buffer, "GET_MAIL", 8) == 0)
        {
            if (!helo_received)
            {
                send(client_socket, "403 FORBIDDEN. MUST SEND HELO FIRST\n", 36, 0);
                continue;
            }

            char get_recipient[BUFFER_SIZE];
            int get_id;
            sscanf(buffer, "GET_MAIL %s %d", get_recipient, &get_id);

            if (!check_valid_email(get_recipient))
            {
                send(client_socket, "400 ERR. INVALID EMAIL FORMAT\n", 29, 0);
                continue;
            }

            char get_domain[BUFFER_SIZE];
            memset(get_domain, 0, BUFFER_SIZE);
            extract_domain(get_recipient, get_domain, sizeof(get_domain));

            if (strcmp(get_domain, helo_domain) != 0)
            {
                send(client_socket, "403 FORBIDDEN: GET_MAIL DOMAIN MUST MATCH HELO DOMAIN\n", 53, 0);
                continue;
            }

            get_email(client_socket, get_recipient, get_id);
            printf("GET_MAIL %s %d\n", get_recipient, get_id);
            printf("EMAIL WITH ID %d SENT.\n", get_id);
        }
        else
        {
            send(client_socket, "400 ERR\n", 8, 0);
        }
    }

    close(client_socket);
    printf("CLIENT DISCONNECTED. SOCKET: %d\n", client_socket);
    return NULL;
}

int main(int argc, char const *argv[])
{
    int port = 2525;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    pthread_t tid;

    // create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    printf("SERVER LISTENING ON PORT %d\n", port);

    mkdir(MAILBOX_DIR, 0777); // make if doesn't exist

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("CLIENT CONNECTED: IP ADDR: %s SOCKET: %d\n", inet_ntoa(client_addr.sin_addr), client_socket);

        // Allocate memory for the socket descriptor to pass to the thread
        int *client_sock = malloc(sizeof(int));
        if (client_sock == NULL)
        {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }
        *client_sock = client_socket;

        // Create a thread to handle the client
        if (pthread_create(&tid, NULL, handle_client, client_sock) != 0)
        {
            perror("Thread creation failed");
            free(client_sock);
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}