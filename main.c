#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define WIDTH 800
#define HEIGHT 600
#define BUFFER_SIZE 128
#define SPEED_FACTOR 2
#define NUM_NPCS 20
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1" // Localhost for testing, change to actual server IP when deploying
#define MAX_PLAYERS 10		  // Maximum number of players

#define MSG_LOCK_REQUEST "lock_request"
#define MSG_LOCK_GRANTED "lock_granted"
#define MSG_LOCK_DENIED "lock_denied"
#define MSG_LOCK_NOTIFICATION "lock_notification"
#define MSG_LOCK_RELEASE "lock_release"

pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

int sockfd;
struct sockaddr_in serverAddr;
int localPlayerID = -1; // Initialize localPlayerID to -1 (not set)
int hasLock = 0;		// Global variable to track lock ownership
int lockRequested = 0;

typedef struct
{
	char name[BUFFER_SIZE];
	int x;
	int y;
	int vx;
	int vy;
	int radius;
	unsigned int speed;
	Color color;
	bool active;
} Circle;

Circle players[MAX_PLAYERS]; // Array to store player data

void setupNetworking()
{
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Socket created successfully.\n");
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

	if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
	{
		perror("Connection failed");
		exit(EXIT_FAILURE);
	}

	// Set to non-blocking mode
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

void sendMessage(const char *message)
{
	send(sockfd, message, strlen(message), 0);
	printf("Sent message: %s\n", message); // Debug print statement
}

int receiveMessage(char *buffer, size_t bufferSize)
{
	int recv_len;

	// Attempt to receive data from the socket
	recv_len = recv(sockfd, buffer, bufferSize, 0);

	if (recv_len > 0)
	{
		buffer[recv_len] = '\0'; // Null-terminate the received data
		return 1;				 // Data was received
	}

	return 0; // No data received
}

void parseGameState(char *buffer)
{
	int id, x, y, radius, speed;
	if (sscanf(buffer, "id=%d,x=%d,y=%d,radius=%d,speed=%d", &id, &x, &y, &radius, &speed) == 5)
	{
		if (id >= 0 && id < MAX_PLAYERS)
		{
			if (!players[id].active)
			{
				players[id].active = true;
				snprintf(players[id].name, BUFFER_SIZE, "Player-%d", id);
				players[id].color = (id == localPlayerID) ? BLUE : RED;
				printf("Player %d activated at %d, %d\n", id, x, y);
			}
			players[id].x = x;
			players[id].y = y;
			players[id].radius = radius;
			players[id].speed = speed;
		}
	}
}

void handleLockResponse(char *buffer)
{
	int id;
	if (sscanf(buffer, "lock_granted,id=%d", &id) == 1)
	{
		if (id == localPlayerID)
		{
			hasLock = 1;
			lockRequested = 0;
			printf("Lock granted to player %d\n", id);
		}
	}
	else if (sscanf(buffer, "lock_notification,id=%d", &id) == 1)
	{
		if (id != localPlayerID)
		{
			hasLock = 0;
		}
		printf("Lock held by player %d\n", id);
	}
	else if (sscanf(buffer, "lock_denied,id=%d", &id) == 1)
	{
		if (id == localPlayerID)
		{
			printf("Lock denied for player %d\n", id);
			lockRequested = 0;
		}
	}
}

void InitPlayers()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		snprintf(players[i].name, BUFFER_SIZE, "Player-%d", i);
		players[i].x = 0;
		players[i].y = 0;
		players[i].vx = 0;
		players[i].vy = 0;
		players[i].radius = 20; // Default size, adjust as needed
		players[i].speed = SPEED_FACTOR;
		players[i].color = (i == localPlayerID) ? BLUE : RED;
		players[i].active = false; // Start as inactive
	}
	// Activate only the local player initially
	players[localPlayerID].active = true;
	players[localPlayerID].x = GetRandomValue(0, WIDTH);
	players[localPlayerID].y = GetRandomValue(0, HEIGHT);
	players[localPlayerID].speed = SPEED_FACTOR;
}

void DrawPlayers()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i].active)
		{
			DrawCircle(players[i].x, players[i].y, players[i].radius, players[i].color);
			DrawText(players[i].name, players[i].x - players[i].radius, players[i].y - 10, 20, WHITE);
		}
	}
}

void requestLock()
{
	pthread_mutex_lock(&clientMutex); // Locking the mutex before requesting the lock
	if (!hasLock && !lockRequested)
	{
		char buffer[BUFFER_SIZE];
		sprintf(buffer, "%s,id=%d", MSG_LOCK_REQUEST, localPlayerID);
		sendMessage(buffer);
		printf("Player %d requested lock\n", localPlayerID); // Debug print statement
		lockRequested = 1;									 // Set lockRequested to prevent repeated requests
	}
	pthread_mutex_unlock(&clientMutex); // Unlocking the mutex after requesting the lock
}

void UpdatePlayerPosition(int index)
{
	if (index != localPlayerID)
		return;

	Circle *player = &players[index];
	int initialX = player->x;
	int initialY = player->y;

	pthread_mutex_lock(&clientMutex); // Locking the mutex before checking key presses and updating position
	if ((IsKeyDown(KEY_D) || IsKeyDown(KEY_A) || IsKeyDown(KEY_W) || IsKeyDown(KEY_S)) && !hasLock && !lockRequested)
	{
		requestLock();
	}

	if (hasLock)
	{
		int moved = 0;

		if (IsKeyDown(KEY_D))
		{
			player->x += player->speed;
			moved = 1;
		}
		if (IsKeyDown(KEY_A))
		{
			player->x -= player->speed;
			moved = 1;
		}
		if (IsKeyDown(KEY_W))
		{
			player->y -= player->speed;
			moved = 1;
		}
		if (IsKeyDown(KEY_S))
		{
			player->y += player->speed;
			moved = 1;
		}

		if (moved)
		{
			char buffer[128];
			sprintf(buffer, "id=%d,x=%d,y=%d,radius=%d,speed=%d", localPlayerID, player->x, player->y, player->radius, player->speed);
			sendMessage(buffer);
			printf("Sent player position: %s\n", buffer); // Debug print statement
		}

		if (!IsKeyDown(KEY_D) && !IsKeyDown(KEY_A) && !IsKeyDown(KEY_W) && !IsKeyDown(KEY_S))
		{
			hasLock = 0;
			lockRequested = 0;
			char buffer[BUFFER_SIZE];
			sprintf(buffer, "%s,id=%d", MSG_LOCK_RELEASE, localPlayerID);
			sendMessage(buffer);
		}
	}

	pthread_mutex_unlock(&clientMutex); // Unlocking the mutex after checking key presses and updating position

	if (player->x != initialX || player->y != initialY)
	{
		printf("Player %d moved from (%d, %d) to (%d, %d)\n", index, initialX, initialY, player->x, player->y);
	}
}

int CheckCollision(Circle c1, Circle c2)
{
	float distance = sqrt(pow(c2.x - c1.x, 2) + pow(c2.y - c1.y, 2));
	return distance < (c1.radius + c2.radius);
}

void ProcessCollisionsBetweenPlayers()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		for (int j = i + 1; j < MAX_PLAYERS; j++)
		{
			if (players[i].active && players[j].active && CheckCollision(players[i], players[j]))
			{
				if (players[i].radius > players[j].radius)
				{
					players[i].radius += 2;
					players[j].active = false;
				}
				else
				{
					players[j].radius += 2;
					players[i].active = false;
				}
			}
		}
	}
}

int main()
{
	InitWindow(WIDTH, HEIGHT, "Multiplayer Game");
	SetTargetFPS(60);
	setupNetworking();

	// Receive unique player ID from the server
	char recvBuffer[BUFFER_SIZE];
	while (localPlayerID == -1)
	{
		sendMessage("request_id");
		if (receiveMessage(recvBuffer, sizeof(recvBuffer)))
		{
			sscanf(recvBuffer, "id=%d", &localPlayerID);
		}
	}

	InitPlayers();

	while (!WindowShouldClose())
	{
		if (receiveMessage(recvBuffer, sizeof(recvBuffer)))
		{
			if (strncmp(recvBuffer, MSG_LOCK_GRANTED, strlen(MSG_LOCK_GRANTED)) == 0 ||
				strncmp(recvBuffer, MSG_LOCK_NOTIFICATION, strlen(MSG_LOCK_NOTIFICATION)) == 0 ||
				strncmp(recvBuffer, MSG_LOCK_DENIED, strlen(MSG_LOCK_DENIED)) == 0)
			{
				handleLockResponse(recvBuffer);
			}
			else
			{
				parseGameState(recvBuffer);
			}
		}

		BeginDrawing();
		ClearBackground(BLACK);

		UpdatePlayerPosition(localPlayerID);
		ProcessCollisionsBetweenPlayers();
		DrawPlayers();

		EndDrawing();
	}

	CloseWindow();
	close(sockfd);
	return 0;
}
