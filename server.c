#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

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
        buffer[n] = '\0';
        handleNewConnection(sockfd, clientAddr);
        // Broadcast received message to all clients
        for (int i = 0; i < numClients; i++)
        {
            sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
        }
    }

    close(sockfd);
    return 0;
}
