/*
==========================================
Assignment 2 Submission
Name: Bhandare Omkar Vijay
Roll number: 22CS30016
Link of the pcap file: https://drive.google.com/drive/folders/17-qoWrRAIoqeW8CF-ic4lxbmPWjweWal?usp=sharing
==========================================
*/

// Include header for standard I/O functions (printf, fopen, etc.)
#include <stdio.h>

// Include header for string handling functions (strlen, strcpy, memset, strncat, strcmp, etc.)
#include <string.h>

// Include header defining data types used in syscalls (like ssize_t)
#include <sys/types.h>

// Include header for definitions for internet operations (functions like htons, htonl, and structures for network addresses)
#include <arpa/inet.h>

// Include header for socket system calls (socket, bind, sendto, recvfrom, etc.)
#include <sys/socket.h>

// Include header that defines the internet address family structure (struct sockaddr_in for IPv4)
#include <netinet/in.h>

// Include header for POSIX operating system API functions (like close)
#include <unistd.h>

// Include header for various utility functions (like exit)
#include <stdlib.h>

// Define the maximum buffer size for data transmission (in characters)
#define MAX 1000

// Define the port number that the server will use for listening (5000)
#define PORT 5000

// Start of the main function
int main()
{
    // Create a UDP socket
    // AF_INET         : Address family for IPv4 addresses.
    // SOCK_DGRAM      : Socket type for UDP (datagram) communications.
    // 0               : Protocol (0 lets the system choose the correct protocol for SOCK_DGRAM, which is UDP).
    int server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Check if the socket was created successfully
    if (server_socket < 0)
    {
        // perror prints the error message based on the last error encountered
        perror("Failed! Socket not created\n");
        // Exit the program with an error code
        exit(EXIT_FAILURE);
    }

    // Declare a structure to hold the server address information
    struct sockaddr_in server_address;

    // Initialize the server_address memory to zero
    memset(&server_address, 0, sizeof(server_address));

    // Set the address family to AF_INET for IPv4
    server_address.sin_family = AF_INET;

    // Set the port number (convert host byte order to network byte order using htons)
    server_address.sin_port = htons(PORT);

    // Set the IP address of the server.
    // INADDR_ANY tells the system to use any available network interface.
    // htonl converts the host long value to network byte order.
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the server_address.
    // bind() associates the socket with the specified local address.
    // Arguments:
    //    server_socket           : The socket file descriptor.
    //    (struct sockaddr *)&server_address : Pointer to the address structure.
    //    sizeof(server_address)  : Size of the address structure.
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Failed! Socket not binded\n");
        exit(EXIT_FAILURE);
    }

    // Declare a sockaddr_in structure to store client's address info when received
    struct sockaddr_in client_address;

    // Initialize the client_address memory to zero
    memset(&client_address, 0, sizeof(client_address));

    // Inform that the server is running
    printf("Server is running...\n");

    // Variable to hold the size of client address structure (used by recvfrom)
    socklen_t len = sizeof(client_address);

    // Create a buffer to hold data received or sent; size defined by MAX
    char buffer[MAX];

    // Zero out the buffer to clear any garbage values
    memset(buffer, 0, MAX);

    // Receive data from a client.
    // Parameters:
    //    server_socket          : The socket file descriptor.
    //    buffer                 : Buffer to store received data.
    //    sizeof(buffer)-1       : Maximum number of bytes to read (leaving room for string termination).
    //    0                      : Flags (none specified).
    //    (struct sockaddr *)&client_address : Pointer to client_address to capture sender's address.
    //    &len                   : Pointer to variable holding the size of client_address.
    int n = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_address, &len);

    // Uncommented diagnostic printf, removed for cleaner output.
    // printf("Received\n");

    // Check if the recvfrom() call was successful
    if (n < 0)
    {
        perror("Error! Filename not received\n");
        exit(EXIT_FAILURE);
    }

    // Add a null terminator at the end of the received data to form a proper C-string.
    buffer[n] = '\0';

    // Uncommented diagnostic printf to display file name, removed for cleaner output.
    // printf("File name: %s\n", buffer);

    // Open the file with the name received in read ("r") mode.
    // fopen() arguments:
    //    buffer : File name (C-string) received from the client.
    //    "r"    : Mode to open file in read-only.
    FILE *fp = fopen(buffer, "r");

    // Check if the file was successfully opened
    if (fp == NULL)
    {
        // If the file is not found, prepare an error message.
        char message[MAX] = "NOTFOUND ";

        // Append the filename to the "NOTFOUND " message.
        // strncat appends the content from buffer to message without exceeding MAX.
        strncat(message, buffer, MAX - strlen(message) - 1);

        // Send the error message back to the client.
        // Arguments are similar as before:
        //    server_socket: the socket file descriptor.
        //    message      : data to send.
        //    strlen(message) : length of the message.
        //    0            : flags.
        //    (struct sockaddr *)&client_address : pointer with client address.
        //    len          : size of the client address structure.
        sendto(server_socket, message, strlen(message), 0, (struct sockaddr *)&client_address, len);

        // Inform via console that file was not found.
        printf("File not found. Server terminating.\n");

        // Close the socket and terminate
        close(server_socket);
        return 0;
    }
    else
    {
        // If the file exists, the server is ready to start file transmission.

        // Clear the buffer before reading file content.
        memset(buffer, 0, MAX);

        // Read the first word from the file using fscanf.
        // "%s" reads a whitespace separated string.
        // If end-of-file is reached immediately, fscanf() returns EOF.
        if (fscanf(fp, "%s", buffer) == EOF)
        {
            // Ensure any buffered output is written
            fflush(stdout);

            // Inform that end-of-file (or no content) was reached
            printf("finish from eof\n");

            // Mark the transmission as finished by setting buffer to "FINISH"
            strcpy(buffer, "FINISH");

            // Send the "FINISH" message to the client as indication of no content
            sendto(server_socket, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&client_address, len);

            // Print completion message
            printf("Empty file. Transmission complete. Server terminating.\n");

            // Close file pointer and socket before exit
            fclose(fp);
            close(server_socket);
            return 0;
        }
        // Send the first word from the file to the client.
        sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, len);

        // Begin an infinite loop to service subsequent word requests from the client
        while (1)
        {
            // Clear the buffer to prepare for new incoming data from client
            memset(buffer, 0, MAX);

            // Wait for the client’s next message.
            // This can be a request for a particular word number.
            int n = recvfrom(server_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_address, &len);

            // If no data is received (n < 0), continue (could add error handling)
            if (n < 0)
                continue;

            // Terminate the received buffer string with a null character.
            buffer[n] = '\0';

            // Check if the received message starts with "WORD"
            // strncmp compares the first 4 characters of buffer with "WORD"
            if (strncmp(buffer, "WORD", 4) == 0)
            {
                // Print the full message received from the client.
                printf("Message from client : %s\n", buffer);

                // Extract the number following "WORD" in the message.
                // This assumes that the client sends something like: "WORD<number>"
                char *num = buffer + 4;

                // Pointer to keep track of where the number conversion stopped
                char *endptr;

                // Convert the extracted substring to a long integer.
                // strtol arguments:
                //    num   : The starting string pointer of the number portion.
                //    &endptr : Pointer to the character after the last digit.
                //    10    : Base 10 conversion.
                long word_no = strtol(num, &endptr, 10);

                // Check if conversion failed (i.e., no digits were found) or if word_no is less or equal to zero.
                if (endptr == num || word_no <= 0)
                {
                    // Flush any pending output
                    fflush(stdout);

                    // Inform about the illegal word number.
                    printf("finish from illegal word no.\n");

                    // Prepare the "FINISH" signal
                    strcpy(buffer, "FINISH");

                    // Send the "FINISH" message to the client to indicate termination.
                    sendto(server_socket, buffer, strlen(buffer), 0,
                           (struct sockaddr *)&client_address, len);

                    // Print termination message
                    printf("Invalid word number received. Transmission complete. Server terminating.\n");

                    // Break out of the loop to terminate the server.
                    break;
                }

                // Rewind the file pointer to the beginning of the file.
                // This is necessary because the file reading starts from the top.
                rewind(fp);
                int cnt = 0, found = 0;

                // Clear the buffer before reading new content.
                memset(buffer, 0, MAX);

                // Loop through the file word by word using fscanf.
                while (fscanf(fp, "%s", buffer) != EOF)
                {
                    // Increment the counter for each word read.
                    cnt++;

                    // When cnt equals word_no + 1,
                    // note: the first sendto was already done with the first word, so an offset of +1 is added.
                    if (cnt == word_no + 1)
                    {
                        // Flush any pending output
                        fflush(stdout);

                        // Print the word that will be sent to the client.
                        printf("In server sending : %s\n", buffer);

                        // Send the requested word to the client.
                        sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, len);

                        // If the sent word is "FINISH", then this marks the end of transmission.
                        if (strcmp(buffer, "FINISH") == 0)
                        {
                            printf("End of file reached. Transmission complete. Server terminating.\n");
                            close(server_socket);
                            return 0;
                        }
                        // Exit the inner loop as the desired word has been sent.
                        break;
                    }
                }
            }
        }
    }

    // If the file was open, close it.
    if (fp != NULL)
        fclose(fp);

    // Close the server socket before terminating the program.
    close(server_socket);

    // Return 0 to indicate successful termination of the program.
    return 0;
}
