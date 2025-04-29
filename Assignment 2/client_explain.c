/*
==========================================
Assignment 2 Submission
Name: Bhandare Omkar Vijay
Roll number: 22CS30016
Link of the pcap file: https://drive.google.com/drive/folders/17-qoWrRAIoqeW8CF-ic4lxbmPWjweWal?usp=sharing
==========================================
*/

// Include standard input/output functions header
#include <stdio.h>

// Include string manipulation functions header
#include <string.h>

// Include system types definitions header
#include <sys/types.h>

// Include internet address family functions header
#include <arpa/inet.h>

// Include socket programming functions header
#include <sys/socket.h>

// Include internet address structures header
#include <netinet/in.h>

// Include POSIX operating system API header
#include <unistd.h>

// Include standard library functions header
#include <stdlib.h>

// Define maximum buffer size constant
#define MAX 1000

// Define server port number constant
#define PORT 5000

int main()
{
    // Create UDP socket
    // AF_INET: IPv4 protocol
    // SOCK_DGRAM: UDP socket type
    // 0: Default protocol
    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Check if socket creation failed
    if (client_socket < 0)
    {
        perror("Failed! Socket not created\n");
        exit(EXIT_FAILURE);
    }

    // Declare server address structure
    struct sockaddr_in server_address;

    // Initialize server address structure with zeros
    memset(&server_address, 0, sizeof(server_address));

    // Set address family to IPv4
    server_address.sin_family = AF_INET;

    // Set port number (convert to network byte order)
    server_address.sin_port = htons(PORT);

    // Set IP address to localhost (127.0.0.1)
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Store length of server address structure
    socklen_t len = sizeof(server_address);

    // Declare buffers for data and filename
    char buffer[MAX], file_name[MAX];

    // Prompt user for file number input
    int nx;
    printf("Enter the file number from 1 to 5 (both inclusive) : ");
    scanf("%d", &nx);

    // Validate file number range
    if (nx < 1 || nx > 5)
    {
        printf("Invalid file number. Exiting...\n");
        close(client_socket);
        return 0;
    }

    // Create filename string using format: 22CS30016_File<number>.txt
    snprintf(file_name, MAX, "22CS30016_File%d.txt", nx);

    // Send filename to server
    // Arguments: socket, data, data length, flags, server address, address length
    if (sendto(client_socket, file_name, strlen(file_name), 0,
               (struct sockaddr *)&server_address, len) < 0)
    {
        perror("Error sending filename");
        exit(EXIT_FAILURE);
    }

    // Clear buffer for receiving server response
    memset(buffer, 0, MAX);

    // Receive response from server
    // Arguments: socket, buffer, buffer size-1, flags, server address, address length
    int n = recvfrom(client_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&server_address, &len);

    // Check for receive error
    if (n < 0)
    {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }

    // Null-terminate received string
    buffer[n] = '\0';

    // Check if file was not found on server
    if (strncmp(buffer, "NOTFOUND", 8) == 0)
    {
        printf("Server response: %s\n", buffer);
        printf("Error: FILE NOT FOUND. Exiting...\n");
        close(client_socket);
        return 0;
    }

    // Open output file for writing
    FILE *fp = fopen("22CS30016_Output_File", "w");

    // Check if file creation failed
    if (fp == NULL)
    {
        perror("Failed! File not created\n");
        close(client_socket);
        return 0;
    }

    // Print and write first word received from server
    printf("Server response: %s\n", buffer);
    fprintf(fp, "%s\n", buffer);

    // Initialize word counter
    int word_index = 1;

    // Main communication loop
    while (1)
    {
        // Clear buffer for new request
        memset(buffer, 0, MAX);

        // Create WORD request string
        snprintf(buffer, MAX, "WORD%d", word_index);

        // Send WORD request to server
        if (sendto(client_socket, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&server_address, len) < 0)
        {
            perror("Error sending WORD request");
            break;
        }

        // Clear buffer for server response
        memset(buffer, 0, MAX);

        // Receive word from server
        n = recvfrom(client_socket, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *)&server_address, &len);

        // Check for receive error
        if (n < 0)
        {
            perror("Error receiving word");
            break;
        }

        // Null-terminate received string
        buffer[n] = '\0';

        // Check if server sent FINISH signal
        if (strcmp(buffer, "FINISH") == 0)
        {
            fprintf(fp, "%s\n", buffer);
            printf("Received FINISH. Closing connection.\n");
            break;
        }

        // Write received word to file and display
        fprintf(fp, "%s\n", buffer);
        printf("Received word: %s\n", buffer);

        // Increment word counter
        word_index++;
    }

    // Close output file
    fclose(fp);

    // Close socket
    close(client_socket);

    // Print success message
    printf("File output.txt created successfully\n");
    return 0;
}
