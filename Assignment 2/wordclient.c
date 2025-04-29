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
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0)
    {
        perror("Failed! Socket not created\n");
        exit(EXIT_FAILURE);
    }

    // for client also create the server address and bind it to the socket
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    socklen_t len = sizeof(server_address);
    char buffer[MAX], file_name[MAX];

    // prompt the user to enter the file name
    int nx;
    printf("Enter the file number from 1 to 5 (both inclusive) : ");
    scanf("%d", &nx);
    if(nx < 1 || nx > 5)
    {
        printf("Invalid file number. Exiting...\n");
        close(client_socket);
        return 0;
    }
    snprintf(file_name, MAX, "22CS30016_File%d.txt", nx);

    if (sendto(client_socket, file_name, strlen(file_name), 0,
               (struct sockaddr *)&server_address, len) < 0)
    {
        perror("Error sending filename");
        exit(EXIT_FAILURE);
    }

    memset(buffer, 0, MAX);
    int n = recvfrom(client_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&server_address, &len);
    if (n < 0)
    {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0'; // end the buffer string

    if (strncmp(buffer, "NOTFOUND", 8) == 0)
    {
        printf("Server response: %s\n", buffer);
        printf("Error: FILE NOT FOUND. Exiting...\n");
        close(client_socket);
        return 0;
    }

    FILE *fp = fopen("22CS30016_Output_File", "w");
    if (fp == NULL)
    {
        perror("Failed! File not created\n");
        close(client_socket);
        return 0;
    }

    printf("Server response: %s\n", buffer);
    fprintf(fp, "%s\n", buffer);

    int word_index = 1;
    while (1)
    {
        // Prepare and send WORDi request
        memset(buffer, 0, MAX);
        snprintf(buffer, MAX, "WORD%d", word_index);
        if (sendto(client_socket, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&server_address, len) < 0)
        {
            perror("Error sending WORD request");
            break;
        }

        memset(buffer, 0, MAX);
        // Receive word from the server
        n = recvfrom(client_socket, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *)&server_address, &len);
        if (n < 0)
        {
            perror("Error receiving word");
            break;
        }
        buffer[n] = '\0';

        // Check for FINISH message
        if (strcmp(buffer, "FINISH") == 0)
        {
            fprintf(fp, "%s\n", buffer); // Write FINISH to the file
            printf("Received FINISH. Closing connection.\n");
            break;
        }

        // Write the word to the file and display it
        fprintf(fp, "%s\n", buffer);
        printf("Received word: %s\n", buffer);

        // Increment word index
        word_index++;
    }

    // close the file and socket
    fclose(fp);
    close(client_socket);

    printf("File output.txt created successfully\n");
    return 0;
}