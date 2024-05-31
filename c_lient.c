#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_file(FILE *fp, int sockfd, char *filename)
{
    char data[BUFFER_SIZE] = {0};

    puts(filename);

    // Send filename
    send(sockfd, filename, strlen(filename), 0);

    puts("Filename sent.");

    // Get file size
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Send file size
    char clen[128];
    sprintf(clen, "%d", len);
    puts(clen);
    send(sockfd, clen, strlen(clen), 0);

    puts("File size sent.");

    // Send file contents
    while (fread(data, sizeof(char), BUFFER_SIZE, fp) > 0)
    {
        puts("Sending data...");
        if (send(sockfd, data, strlen(data), 0) == -1)
        {
            perror("Error in sending file.");
            exit(1);
        }
        memset(data, 0, BUFFER_SIZE);
    }
}

void write_file(int sockfd, char *oldFilename)
{
    int n;
    FILE *fp;
    char filename[BUFFER_SIZE] = "";
    strcat(filename, oldFilename);
    strcat(filename, ".enc");
    char buffer[BUFFER_SIZE];

    // Open file to write
    fp = fopen(filename, "w");
    if (fp == NULL)
    {
        perror("[-]Error in creating file.");
        exit(1);
    }

    // Receive length
    int len;
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    len = atoi(buffer);
    memset(buffer, 0, BUFFER_SIZE);

    // Send SIGOKRECV
    send(sockfd, "SIGOKRECV", strlen("SIGOKRECV"), 0);

    // Receive file contents
    while (len > 0)
    {
        n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            fclose(fp);
            exit(1);
        }
        fprintf(fp, "%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);
        len -= n;
        printf("%d\n", len);
    }

    fclose(fp);
    return;
}

int main()
{
    FILE *fp;
    char filename[BUFFER_SIZE];

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("[+]Server socket created successfully.\n");

    // Connect
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    printf("[+]Connected to Server.\n");

    recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (strcmp(buffer, "SIGFULL") == 0)
    {
        printf("Server is full. Try again later.\n");
        close(sockfd);
        return -1;
    }
    else if (strcmp(buffer, "SIGOK") == 0)
    {
        memset(buffer, 0, BUFFER_SIZE);
    }

    // Get file name and open it
    printf("Enter the filename to send: ");
    scanf("%s", filename);

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Cannot open file.\n");
        close(sockfd);
        return -1;
    }

    // Send and receive file
    send_file(fp, sockfd, filename);
    printf("File data sent successfully.\n");

    write_file(sockfd, filename);
    printf("File data received successfully.\n");

    // Close file and socket
    fclose(fp);
    close(sockfd);

    return 0;
}