#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h> // Add this include

pthread_mutex_t lockMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lockCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

#define PORT 8888
#define BUFFER_SIZE 128
#define MAX_PLAYERS 10

typedef struct
{
    int sock;
    struct sockaddr_in addr;
    int id;
} Client;

Client clients[MAX_PLAYERS];
int numClients = 0;
int currentLockHolder = -1; // No lock holder initially

void addClient(int clientSock, struct sockaddr_in clientAddr)
{
    pthread_mutex_lock(&clientMutex);
    if (numClients < MAX_PLAYERS)
    {
        clients[numClients].sock = clientSock;
        clients[numClients].addr = clientAddr;
        clients[numClients].id = numClients;
        printf("New client connected with ID %d\n", numClients); // Log new client connection
        numClients++;
    }
    pthread_mutex_unlock(&clientMutex);
}

int findClientIndex(int clientSock)
{
    for (int i = 0; i < numClients; i++)
    {
        if (clients[i].sock == clientSock)
        {
            return i;
        }
    }
    return -1;
}

void handleLockRequest(int clientSock, int clientID)
{
    char response[BUFFER_SIZE];

    pthread_mutex_lock(&lockMutex); // Lock the mutex before accessing shared state

    if (currentLockHolder == -1)
    {
        // Grant the lock
        currentLockHolder = clientID;
        snprintf(response, BUFFER_SIZE, "lock_granted,id=%d", clientID);
        send(clientSock, response, strlen(response), 0);
        printf("Lock granted to player %d\n", clientID); // Add this debug print

        // Notify all clients
        snprintf(response, BUFFER_SIZE, "lock_notification,id=%d", clientID);
        for (int i = 0; i < numClients; i++)
        {
            send(clients[i].sock, response, strlen(response), 0);
        }
    }
    else
    {
        // Deny the lock
        snprintf(response, BUFFER_SIZE, "lock_denied,id=%d", clientID);
        send(clientSock, response, strlen(response), 0);
        printf("Lock denied for player %d\n", clientID); // Add this debug print
    }

    pthread_mutex_unlock(&lockMutex); // Unlock the mutex after accessing shared state
}

void handleLockRelease(int clientID)
{
    pthread_mutex_lock(&lockMutex); // Lock the mutex before accessing shared state

    if (currentLockHolder == clientID)
    {
        currentLockHolder = -1;
        printf("Lock released by player %d\n", clientID); // Add this debug print
        pthread_cond_signal(&lockCond);                   // Signal other threads waiting for the lock
    }

    pthread_mutex_unlock(&lockMutex); // Unlock the mutex after accessing shared state
}

void handleNewConnection(int clientSock, struct sockaddr_in clientAddr)
{
    addClient(clientSock, clientAddr);
    int clientIndex = numClients - 1;
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "id=%d", clientIndex);
    send(clientSock, buffer, strlen(buffer), 0);
    printf("Assigned ID %d to new client\n", clientIndex); // Debug print statement
}

void handleMessage(int clientSock, char *buffer)
{
    int clientID;
    if (strncmp(buffer, "request_id", strlen("request_id")) == 0)
    {
        handleNewConnection(clientSock, clients[findClientIndex(clientSock)].addr);
    }
    else if (sscanf(buffer, "lock_request,id=%d", &clientID) == 1)
    {
        printf("Received lock request from player %d\n", clientID); // Add this debug print
        handleLockRequest(clientSock, clientID);
    }
    else if (sscanf(buffer, "lock_release,id=%d", &clientID) == 1)
    {
        printf("Received lock release from player %d\n", clientID); // Add this debug print
        handleLockRelease(clientID);
    }
    else if (sscanf(buffer, "id=%d,", &clientID) == 1)
    {
        pthread_mutex_lock(&lockMutex); // Lock the mutex before checking lock holder

        if (currentLockHolder == clientID)
        {
            printf("Broadcasting message from player %d\n", clientID);
            for (int i = 0; i < numClients; i++)
            {
                send(clients[i].sock, buffer, strlen(buffer), 0);
            }
        }
        else
        {
            printf("Player %d tried to send an update without holding the lock\n", clientID);
        }

        pthread_mutex_unlock(&lockMutex); // Unlock the mutex after checking lock holder
    }
}

void *handleClient(void *arg)
{
    int clientIndex = *((int *)arg);
    int clientSock = clients[clientIndex].sock;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        int n = recv(clientSock, buffer, BUFFER_SIZE, 0);
        if (n <= 0)
        {
            // Client disconnected or error occurred
            printf("Client %d disconnected\n", clientIndex);
            close(clientSock);
            pthread_mutex_lock(&clientMutex);
            numClients--;
            pthread_mutex_unlock(&clientMutex);
            pthread_exit(NULL);
        }
        buffer[n] = '\0';
        handleMessage(clientSock, buffer);
    }

    return NULL;
}

int main()
{
    int serverSock, clientSock;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_len = sizeof(clientAddr);

    if ((serverSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSock, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind failed");
        close(serverSock);
        exit(EXIT_FAILURE);
    }

    listen(serverSock, MAX_PLAYERS);
    printf("Server is running...\n");

    pthread_t threads[MAX_PLAYERS];
    int threadArgs[MAX_PLAYERS];

    while (1)
    {
        clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addr_len);
        if (clientSock < 0)
        {
            perror("Accept failed");
            continue;
        }

        addClient(clientSock, clientAddr);

        // Create a new thread to handle the client
        threadArgs[numClients - 1] = numClients - 1;
        if (pthread_create(&threads[numClients - 1], NULL, handleClient, &threadArgs[numClients - 1]) != 0)
        {
            perror("Failed to create thread");
            close(clientSock);
            pthread_mutex_lock(&clientMutex);
            numClients--;
            pthread_mutex_unlock(&clientMutex);
        }
    }

    close(serverSock);
    return 0;
}