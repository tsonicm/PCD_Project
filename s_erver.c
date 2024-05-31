#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define THREAD_POOL_SIZE 32
#define UNIX_PATH "/tmp/ux.sock"

struct threadPoolData
{
    int *clientSock;
    pthread_t *threadID;
    bool isRunning;
    bool isConnected;
    char *fileName;
    int fileSize;
};

struct adminData
{
    int *clientSock;
    pthread_t *threadPool;
    int *adminSock;
    struct threadPoolData *threadPoolData;
};

bool isAdminConnected = false;
int thread_index = -1;

void sendServerInfo(struct adminData *adminData, int *clientSock)
{
    // Iterate through threadPoolData and check how many threads are connected;
    int usedThreads = 0;
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        if (adminData->threadPoolData[i].isConnected)
        {
            usedThreads++;
        }
    }
    char buffer[BUFFER_SIZE] = {0};

    // Send threadPoolSize
    snprintf(buffer, BUFFER_SIZE, "%d %d", THREAD_POOL_SIZE, usedThreads);
    if (send(*clientSock, buffer, strlen(buffer), 0) < 0)
    {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }
}

void runningThreads(struct adminData *adminData, int *clientSock)
{
    // Iterate through threadPoolData and check which threads are running;
    char *threadIDs = malloc(sizeof(char) * (THREAD_POOL_SIZE * 2));
    int runningThreads = 0;
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        if (adminData->threadPoolData[i].isRunning)
        {
            snprintf(threadIDs, 2, "%d ", i);
            runningThreads++;
        }
    }

    if (runningThreads == 0)
    {
        send(*clientSock, "No threads running", 18, 0);
        return;
    }
    else
    {
        // Send running threads
        if (send(*clientSock, threadIDs, strlen(threadIDs), 0) < 0)
        {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }
    }
    free(threadIDs);
}

void send_file(FILE *fp, int *sockfd)
{
    char data[BUFFER_SIZE] = {0};

    // Get length
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Send length
    char clen[128];
    sprintf(clen, "%d", len);
    send(*sockfd, clen, strlen(clen), 0);

    // Receive SIGOKRECV
    char buffer[BUFFER_SIZE] = {0};
    if (recv(*sockfd, buffer, BUFFER_SIZE, 0) < 0)
    {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }
    if (strncmp(buffer, "SIGOKRECV", 9) != 0)
    {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }

    // Send file contents
    while (fread(data, sizeof(char), BUFFER_SIZE, fp) > 0)
    {
        if (send(*sockfd, data, strlen(data), 0) == -1)
        {
            perror("Error in sending file.");
            exit(1);
        }
        memset(data, 0, BUFFER_SIZE);
    }
}

void write_file(int *sock, char *fileName, int *fileSize)
{
    char buffer[BUFFER_SIZE] = {0};
    int bytes_read;
    char filename[BUFFER_SIZE] = {0};
    int len;

    // Receive filename
    recv(*sock, filename, BUFFER_SIZE, 0);
    strcpy(fileName, filename);
    printf("[%d] Filename: %s\n", *sock, filename);

    // Receive file size
    recv(*sock, buffer, BUFFER_SIZE, 0);
    len = atoi(buffer);
    memset(buffer, 0, BUFFER_SIZE);
    *fileSize = len;
    printf("[%d] File size: %d\n", *sock, len);

    // Create temporary file to encrypt
    char *tmpFileName = malloc(sizeof(char) * 9);
    snprintf(tmpFileName, 9, "tmp%d.txt", *sock);
    FILE *tmpfile = fopen(tmpFileName, "w");
    if (tmpfile == NULL)
    {
        printf("Cannot open file.\n");
        close(*sock);
        exit(1);
    }
    else
    {
        printf("[%d] File created.\n", *sock);
    }

    // Write file contents from client to file
    while (len > 0)
    {
        // printf("\rWriting file... %c", "|/-\\"[(int)time(NULL) % 4]);
        bytes_read = recv(*sock, buffer, BUFFER_SIZE, 0);
        if (bytes_read <= 0)
        {
            fclose(tmpfile);
            exit(1);
        }

        fprintf(tmpfile, "%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);

        len -= bytes_read;
    }
    printf("[%d] File written.\n", *sock);
    fclose(tmpfile);
}

void *handle_client(void *threadData)
{
    struct threadPoolData *threadPoolData = (struct threadPoolData *)threadData;

    // Get file from client
    write_file(threadPoolData->clientSock, threadPoolData->fileName, &threadPoolData->fileSize);

    // Set thread as running
    threadPoolData->isRunning = true;

    // Create temporary file to encrypt
    char *tmpFileName = malloc(sizeof(char) * 9);
    snprintf(tmpFileName, 9, "tmp%d.txt", *threadPoolData->clientSock);
    FILE *tmpfile = fopen(tmpFileName, "r");
    if (tmpfile == NULL)
    {
        printf("Cannot open file.\n");
        close(*threadPoolData->clientSock);
        return NULL;
    }

    // Send file
    send_file(tmpfile, threadPoolData->clientSock);
    fclose(tmpfile);

    // Set thread as not running
    threadPoolData->isRunning = false;

    // Set thread as not connected
    threadPoolData->isConnected = false;

    // Clear file name and size
    threadPoolData->fileName = NULL;
    threadPoolData->fileSize = 0;

    // Close socket
    close(*threadPoolData->clientSock);
    thread_index--;
    remove(tmpFileName);
    return NULL;
}

void *handleAdmin(void *data)
{
    struct adminData *adminData = (struct adminData *)data;
    int *adminSock = adminData->adminSock;
    pthread_t *threadPool = adminData->threadPool;
    int *clientSock = adminData->clientSock;

    struct sockaddr_un adminClientAddr;
    socklen_t adminClientLen = sizeof(adminClientAddr);

adminConnection:
    if ((*clientSock = accept(*adminSock, (struct sockaddr *)&adminClientAddr, &adminClientLen)) < 0)
    {
        fprintf(stderr, "Accept failed with err %d\n", errno);
        exit(EXIT_FAILURE);
    }
    else
    {
        isAdminConnected = true;
        printf("[+]Admin connected\n");
    }

    // Iterate through threadPoolData and check how many threads are connected;
    int usedThreads = 0;
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        if (adminData->threadPoolData[i].isConnected)
        {
            usedThreads++;
        }
    }
    char buffer[BUFFER_SIZE] = {0};

    // Send threadPoolSize
    snprintf(buffer, BUFFER_SIZE, "%d %d", THREAD_POOL_SIZE, usedThreads);
    if (send(*clientSock, buffer, strlen(buffer), 0) < 0)
    {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if (recv(*clientSock, buffer, sizeof(buffer), 0) < 0)
        {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        if (strncmp(buffer, "stop", 4) == 0)
        {
            printf("[-]Stopping server...\n");

            for (int i = 0; i < THREAD_POOL_SIZE; i++)
            {
                pthread_kill(threadPool[i], SIGKILL);
            }
            isAdminConnected = false;
            close(*clientSock);
            unlink(UNIX_PATH);
            remove(UNIX_PATH);
            return NULL;
        }

        if (strncmp(buffer, "disconnect", 10) == 0)
        {
            printf("[-]Disconnecting admin...\n");
            send(*clientSock, "disconnecting...", 13, 0);
            isAdminConnected = false;
            goto adminConnection;
        }

        printf("Admin > %s", buffer);

        if (strncmp(buffer, "stats", 5) == 0)
        {
            sendServerInfo(adminData, clientSock);
        }
        else if (strncmp(buffer, "running", 7) == 0)
        {
            runningThreads(adminData, clientSock);
        }
        else
        {
            if (send(*clientSock, buffer, strlen(buffer), 0) < 0)
            {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
        }

        memset(buffer, 0, sizeof(buffer));
    }
}

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    int adminClientFD;
    struct sockaddr_un adminClientAddr;
    socklen_t adminClientLen = sizeof(adminClientAddr);
    bool connected = false;

    int adminSockFD;
    struct sockaddr_un adminServerAddr;

    struct adminData adminData;

    // create socket
    if ((adminSockFD = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // set server details
    adminServerAddr.sun_family = AF_UNIX;
    snprintf(adminServerAddr.sun_path, sizeof(adminServerAddr.sun_path), "%s", UNIX_PATH);

    // bind socket
    if (bind(adminSockFD, (struct sockaddr *)&adminServerAddr, sizeof(adminServerAddr)) < 0)
    {
        fprintf(stderr, "Bind failed with err %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // listen for new connections and limit the connections to 1
    if (listen(adminSockFD, 1) < 0)
    {
        fprintf(stderr, "Listen failed with err %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // Threadpool
    pthread_t thread_pool[THREAD_POOL_SIZE], adminThread;
    struct threadPoolData threadPoolData[THREAD_POOL_SIZE];

    // Initialize threadPoolData
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        threadPoolData[i].clientSock = NULL;
        threadPoolData[i].threadID = NULL;
        threadPoolData[i].isRunning = false;
        threadPoolData[i].isConnected = false;
        threadPoolData[i].fileName = NULL;
        threadPoolData[i].fileSize = 0;
    }

    // Creating socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind to the port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+]Server is listening on port %d\n", PORT);

    while (1)
    {
        if (!isAdminConnected)
        {
            adminData.adminSock = &adminSockFD;
            adminData.threadPool = thread_pool;
            adminData.clientSock = &adminClientFD;
            adminData.threadPoolData = threadPoolData;
            pthread_create(&adminThread, NULL, handleAdmin, (void *)&adminData);
        }

        // Accept connection
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
        if (new_socket < 0)
        {
            perror("Server accept failed");
            continue;
        }

        // Increment thread index
        if (++thread_index >= THREAD_POOL_SIZE)
        {
            send(new_socket, "SIGFULL", 7, 0);
            close(new_socket);
            thread_index--;
        }
        else
        {
            printf("[+]Client connected\n");
            send(new_socket, "SIGOK", 5, 0);

            // Populate thread pool data
            threadPoolData[thread_index].clientSock = &new_socket;
            threadPoolData[thread_index].threadID = &thread_pool[thread_index];
            threadPoolData[thread_index].isConnected = true;
            threadPoolData[thread_index].fileName = malloc(BUFFER_SIZE);

            printf("[+]Creating thread for client with ID %d\n", new_socket);

            // Create thread
            pthread_create(&thread_pool[thread_index], NULL, handle_client, (void *)&threadPoolData[thread_index]);

            pthread_detach(thread_pool[thread_index]);
        }
    }

    close(server_fd);
    return 0;
}
