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

// custom function to check for correct key
int check(char *key)
{
    if (strlen(key) != 26)
        return 0;
    for (int i = 0; i < 26; i++)
    {
        if (key[i] >= 'a' && key[i] <= 'z')
            return 0;
    }
    return 1;
}
int main()
{
    // create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Error creating socket at client side.\n");
        exit(0);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error connecting client to server.\n");
        exit(0);
    }

    while (1)
    {
        // take input the file name to be encrypted
        char f[MAX_FILENAME_LENGTH];
        printf("Enter the filename to be encrypted : ");
        scanf("%s", f);

        FILE *file = fopen(f, "r");
        while (file == NULL)
        {
            printf("NOTFOUND %s\n", f);
            printf("Enter the filename to be encrypted : ");
            scanf("%s", f);
            file = fopen(f, "r");
        }
        fclose(file);

        // take the correct key, till then throw errors
        char key[27];
        do
        {
            printf("Enter the key (26 characters, no spaces, ALL CAPS) : ");
            scanf("%s", key);
        } while (check(key) == 0);

        // send key to the server
        send(sockfd, key, strlen(key), 0);

        int fd = open(f, O_RDONLY);
        if (fd < 0)
        {
            perror("Error opening file on client side.\n");
            exit(0);
        }

        // send the file to the server in chunks
        char buf[MAX_BUFFER_LENGTH];
        int n;
        while ((n = read(fd, buf, MAX_BUFFER_LENGTH)) > 0)
            send(sockfd, buf, n, 0);
        // custom END character to indicate end of file
        buf[0] = END;
        buf[1] = '\0';
        send(sockfd, buf, 1, 0);
        close(fd);

        // filename of new encrypted file
        char enc_f[MAX_FILENAME_LENGTH];
        strcpy(enc_f, f);
        strcat(enc_f, ".enc");

        fd = open(enc_f, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            perror("Error creating encrypted file at client side.\n");
            exit(0);
        }

        // receive the encrypted files in chunks sent by server
        while ((n = recv(sockfd, buf, MAX_BUFFER_LENGTH, 0)) > 0)
        {
            if (buf[n - 1] == END)
            {
                write(fd, buf, n - 1);
                break;
            }
            write(fd, buf, n);
        }
        close(fd);

        printf("File encrypted successfully.\n");
        printf("Original file : %s\n", f);
        printf("Encrypted file : %s\n", enc_f);

        // for yes/no loop
        char temp[5];
        printf("Do you want to encrypt another file? (Yes/No) : ");
        scanf("%s", temp);
        while (strcasecmp(temp, "No") != 0 && strcasecmp(temp, "Yes") != 0)
        {
            printf("Enter a valid option (Yes/No) : ");
            scanf("%s", temp);
        }
        if (strcasecmp(temp, "No") == 0)
        {
            printf("Closing connection.....\n");
            close(sockfd);
            break;
        }
    }
    return 0;
}