/*
==============================
Name: Omkar Vijay Bhandare
Roll number: 22CS30016
Link to the pcap file: https://drive.google.com/drive/folders/17-qoWrRAIoqeW8CF-ic4lxbmPWjweWal?usp=drive_link
==============================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>

// defining the maximum buffer length and maximum filename length
#define MAX_BUFFER_LENGTH 50
#define MAX_FILENAME_LENGTH 100
#define END '#'

// encrypt the character c using the key
char encrypt(char c, const char *key)
{
    if (!isalpha(c))
        return c;
    if (isupper(c))
        return key[c - 'A'];
    else
        return tolower(key[c - 'a']);
}

int main()
{
    // create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Error creating socket at server.\n");
        exit(0);
    }

    // bind the socket to the port 5000
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(5000);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding the socket in server.\n");
        exit(0);
    }

    // listen for incoming connections (max 5)
    listen(sockfd, 5);
    printf("Server is listening on port 5000.....\n");

    struct sockaddr_in client_addr;
    int newsockfd;

    while (1)
    {
        // accept the incoming connection
        int client_len = sizeof(client_addr);
        printf("Connection Received\n");
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (newsockfd < 0)
        {
            perror("Error accepting the connection at server.\n");
            continue;
        }

        // fork a child process to handle the client
        if (fork() == 0)
        {
            close(sockfd);

            while (1)
            {
                // create temporary file name by ip address and socket
                char temp[MAX_FILENAME_LENGTH];
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                if (snprintf(temp, MAX_FILENAME_LENGTH, "%s.%d.txt", client_ip, ntohs(client_addr.sin_port)) >= MAX_FILENAME_LENGTH)
                {
                    fprintf(stderr, "Error : Filename too large");
                    exit(1);
                }

                char buf[MAX_BUFFER_LENGTH];
                int n = recv(newsockfd, buf, 26, 0);
                if (n <= 0)
                    break;
                char key[27];
                strncpy(key, buf, 26);
                key[26] = '\0';
                printf("Received the key : %s\n", key);
                int f = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (f < 0)
                {
                    perror("Error opening temporary unencrypted file at server.\n");
                    exit(0);
                }

                // go on receiving, if END occurs handle that
                while ((n = recv(newsockfd, buf, MAX_BUFFER_LENGTH - 1, 0)) > 0)
                {
                    buf[n] = '\0';
                    if (strchr(buf, END) != NULL)
                    {
                        char *end_pos = strchr(buf, END);
                        int l = end_pos - buf;
                        if (l > 0)
                            write(f, buf, l);
                        break;
                    }
                    write(f, buf, n);
                }
                close(f);

                // encrypt the file by creating as instructed
                char enc_f[MAX_FILENAME_LENGTH];
                if (snprintf(enc_f, MAX_FILENAME_LENGTH, "%s.enc", temp) >= MAX_FILENAME_LENGTH)
                {
                    fprintf(stderr, "Error : Temporary encrypted filename too long in server.\n");
                    exit(1);
                }

                f = open(temp, O_RDONLY);
                int f_enc = open(enc_f, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (f < 0 || f_enc < 0)
                {
                    perror("Error opening temporary encrypted file on server.\n");
                    exit(0);
                }
                // encrypt character by character
                while ((n = read(f, buf, MAX_BUFFER_LENGTH - 1)) > 0)
                {
                    for (int i = 0; i < n; i++)
                    {
                        buf[i] = encrypt(buf[i], key);
                    }
                    write(f_enc, buf, n);
                }
                close(f);
                close(f_enc);
                // start sending the encrypted file to client
                f_enc = open(enc_f, O_RDONLY);
                while ((n = read(f_enc, buf, MAX_BUFFER_LENGTH - 1)) > 0)
                {
                    send(newsockfd, buf, n, 0);
                }
                // after finish send an exclusive END character to client
                buf[0] = END;
                send(newsockfd, buf, 1, 0);
                close(f_enc);
            }
            // close the socket
            close(newsockfd);
            exit(0);
        }
        // close the socket
        close(newsockfd);
    }
    return 0;
}
