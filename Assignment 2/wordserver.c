/*
==========================================
Assignment 2 Submission
Name: Bhandare Omkar Vijay
Roll number: 22CS30016
Link of the pcap file: https://drive.google.com/drive/folders/17-qoWrRAIoqeW8CF-ic4lxbmPWjweWal?usp=sharing
==========================================
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX 1000
#define PORT 5000

int main()
{
    // create socket
    int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0)
    {
        perror("Failed! Socket not created\n");
        exit(EXIT_FAILURE);
    }

    // bind the server address
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Failed! Socket not binded\n");
        exit(EXIT_FAILURE);
    }

    // for client
    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address));

    printf("Server is running...\n");

    // receive the name of the file from client, and set the client address
    socklen_t len = sizeof(client_address);
    char buffer[MAX];
    memset(buffer, 0, MAX);
    int n = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_address, &len);
    //      recvfrom(int sockfd, void buf[restrict.len], size_t len, int flags, struct sockaddr *_Nullable restrict src_addr, socklen_t *_Nullable restrict addrlen);

    // printf("Received\n");
    if (n < 0)
    {
        perror("Error! Filename not received\n");
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0'; // end the buffer string
    // printf("File name: %s\n", buffer);

    // file handling
    FILE *fp = fopen(buffer, "r");
    if (fp == NULL)
    {
        // the file is not available so the server will sends a NOTFOUND <FILENAME> to the client
        char message[MAX] = "NOTFOUND ";
        strncat(message, buffer, MAX - strlen(message) - 1);
        sendto(server_socket, message, strlen(message), 0, (struct sockaddr *)&client_address, len);
        printf("File not found. Server terminating.\n");
        close(server_socket);
        return 0;
    }
    else
    {
        // the file is availabe to the server and now the communication will start
        memset(buffer, 0, MAX);
        if (fscanf(fp, "%s", buffer) == EOF)
        {
            fflush(stdout);
            printf("finish from eof\n");
            strcpy(buffer, "FINISH");
            sendto(server_socket, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&client_address, len);
            printf("Empty file. Transmission complete. Server terminating.\n");
            fclose(fp);
            close(server_socket);
            return 0;
        }
        // this sendto is for the first word in the file
        sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, len); // sends HELLO

        while (1)
        {
            // receive the message from the client
            memset(buffer, 0, MAX);
            int n = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_address, &len);
            if (n < 0)
                continue;
            buffer[n] = '\0'; // end the buffer string

            if (strncmp(buffer, "WORD", 4) == 0)
            {
                printf("Message from client : %s\n", buffer);
                char *num = buffer + 4;
                char *endptr;
                long word_no = strtol(num, &endptr, 10);

                if (endptr == num || word_no <= 0)
                {
                    fflush(stdout);
                    printf("finish from illegal word no.\n");
                    strcpy(buffer, "FINISH");
                    sendto(server_socket, buffer, strlen(buffer), 0,
                           (struct sockaddr *)&client_address, len);
                    printf("Invalid word number received. Transmission complete. Server terminating.\n");
                    break;
                }

                // start reading the file from the beginning
                rewind(fp);
                int cnt = 0, found = 0;
                memset(buffer, 0, MAX);

                while (fscanf(fp, "%s", buffer) != EOF)
                {
                    cnt++;
                    if (cnt == word_no + 1)
                    {
                        fflush(stdout);
                        printf("In server sending : %s\n", buffer);
                        sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, len);
                        if (strcmp(buffer, "FINISH") == 0) // found the FINISH word in the file, so terminate
                        {
                            printf("End of file reached. Transmission complete. Server terminating.\n");
                            close(server_socket);
                            return 0;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (fp != NULL)
        fclose(fp);
    close(server_socket);

    return 0;
}