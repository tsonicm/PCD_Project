#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define UNIX_PATH "/tmp/ux.sock"

int main()
{
    int sockfd;
    struct sockaddr_un server, client;

    // create socket
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // set server details
    server.sun_family = AF_UNIX;
    snprintf(server.sun_path, sizeof(server.sun_path), "%s", UNIX_PATH);

    // connect to server
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[+]Connected to server\n");
    }

    char buffer[BUFFER_SIZE] = {0};
    int threadSize, usedThreads;

    // Receive message from server about threadPoolSize and usedThreads
    if (recv(sockfd, buffer, BUFFER_SIZE, 0) < 0)
    {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }
    threadSize = atoi(strtok(buffer, " "));
    usedThreads = atoi(strtok(NULL, " "));

    printf("Server > %d / %d threads currently in use\n", usedThreads, threadSize);

    memset(buffer, 0, sizeof(buffer));

    while (true)
    {
        // Send command to server
        printf("Enter message: ");
        fgets(buffer, BUFFER_SIZE, stdin);

        // send message to server
        if (send(sockfd, buffer, strlen(buffer), 0) < 0)
        {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }

        if (strncmp(buffer, "stats", 5) == 0)
        {
            // receive message from server
            if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            threadSize = atoi(strtok(buffer, " "));
            usedThreads = atoi(strtok(NULL, " "));
            printf("Server > %d / %d threads currently in use\n", usedThreads, threadSize);
        }
        else if (strncmp(buffer, "running", 7) == 0)
        {
            // receive message from server
            if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            printf("Running threads: %s\n", buffer);
        }
        else
        {
            // receive message from server
            if (recv(sockfd, buffer, sizeof(buffer), 0) < 0)
            {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            printf("Server: %s", buffer);
        }

        if (strncmp(buffer, "stop", 4) == 0)
        {
            printf("Closing connection\n");
            break;
        }

        if (strncmp(buffer, "disconnect", 10) == 0)
        {
            printf("\nDisconnecting admin\n");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}