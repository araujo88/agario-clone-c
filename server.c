#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t lockMutex = PTHREAD_MUTEX_INITIALIZER;

#define PORT 8888
#define BUFFER_SIZE 128
#define MAX_PLAYERS 10

typedef struct
{
    struct sockaddr_in addr;
    int id;
} Client;

Client clients[MAX_PLAYERS];
int numClients = 0;
int currentLockHolder = -1; // No lock holder initially

void addClient(struct sockaddr_in clientAddr)
{
    if (numClients < MAX_PLAYERS)
    {
        clients[numClients].addr = clientAddr;
        clients[numClients].id = numClients;
        printf("New client connected with ID %d\n", numClients); // Log new client connection
        numClients++;
    }
}

int findClientIndex(struct sockaddr_in clientAddr)
{
    for (int i = 0; i < numClients; i++)
    {
        if (clients[i].addr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
            clients[i].addr.sin_port == clientAddr.sin_port)
        {
            return i;
        }
    }
    return -1;
}

void handleLockRequest(int sockfd, int clientID, struct sockaddr_in clientAddr)
{
    char response[BUFFER_SIZE];

    pthread_mutex_lock(&lockMutex); // Lock the mutex before accessing shared state

    if (currentLockHolder == -1)
    {
        // Grant the lock
        currentLockHolder = clientID;
        snprintf(response, BUFFER_SIZE, "lock_granted,id=%d", clientID);
        sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

        // Log the lock grant
        printf("Lock granted to player %d\n", clientID);

        // Notify all clients
        snprintf(response, BUFFER_SIZE, "lock_notification,id=%d", clientID);
        for (int i = 0; i < numClients; i++)
        {
            sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
        }
    }
    else
    {
        // Lock denied
        snprintf(response, BUFFER_SIZE, "lock_denied,id=%d", clientID);
        sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));

        // Log the lock denial
        printf("Lock denied for player %d\n", clientID);
    }

    pthread_mutex_unlock(&lockMutex); // Unlock the mutex after accessing shared state
}

void handleNewConnection(int sockfd, struct sockaddr_in clientAddr)
{
    int clientIndex = findClientIndex(clientAddr);
    if (clientIndex == -1)
    {
        addClient(clientAddr);
        clientIndex = numClients - 1;
        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "id=%d", clientIndex);
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
        printf("Assigned ID %d to new client\n", clientIndex); // Debug print statement
    }
    else
    {
        printf("Client with ID %d already connected\n", clientIndex); // Debug print statement
    }
}

void handleMessage(int sockfd, char *buffer, struct sockaddr_in clientAddr)
{
    int clientID;
    if (sscanf(buffer, "lock_request,id=%d", &clientID) == 1)
    {
        printf("Received lock request from player %d\n", clientID);
        handleLockRequest(sockfd, clientID, clientAddr);
    }
    else if (sscanf(buffer, "id=%d,", &clientID) == 1)
    {
        pthread_mutex_lock(&lockMutex); // Lock the mutex before checking lock holder

        if (currentLockHolder == clientID)
        {
            printf("Broadcasting message from player %d\n", clientID);
            for (int i = 0; i < numClients; i++)
            {
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
            }
        }
        else
        {
            printf("Player %d tried to send an update without holding the lock\n", clientID);
        }

        pthread_mutex_unlock(&lockMutex); // Unlock the mutex after checking lock holder
    }
}

int main()
{
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_len = sizeof(clientAddr);
    char buffer[BUFFER_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running...\n");

    while (1)
    {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addr_len);
        if (n > 0)
        {
            buffer[n] = '\0';
            handleNewConnection(sockfd, clientAddr);
            handleMessage(sockfd, buffer, clientAddr);
        }
    }

    close(sockfd);
    return 0;
}
