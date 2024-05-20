#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define WIDTH 1000
#define HEIGHT 800
#define MAX_BUFFER_SIZE 128
#define SPEED_FACTOR 2
#define NUM_NPCS 20

typedef struct
{
	char name[MAX_BUFFER_SIZE];
	int x;
	int y;
	int vx;
	int vy;
	int radius;
	unsigned int speed;
	Color color;
	bool active;
} Circle;

Circle npcs[NUM_NPCS]; // Array to store circle data

void CustomLog(int msgType, const char *text)
{
	char timeStr[64] = {0};
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);

	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
	printf("[%s] ", timeStr);

	switch (msgType)
	{
	case LOG_INFO:
		printf("[INFO] : ");
		break;
	case LOG_ERROR:
		printf("[ERROR]: ");
		break;
	case LOG_WARNING:
		printf("[WARN] : ");
		break;
	case LOG_DEBUG:
		printf("[DEBUG]: ");
		break;
	default:
		break;
	}

	printf("%s\n", text);
}

void InitNpcs()
{
	for (int i = 0; i < NUM_NPCS; i++)
	{
		snprintf(npcs[i].name, MAX_BUFFER_SIZE, "NPC-%d", i);
		npcs[i].x = GetRandomValue(0, WIDTH);
		npcs[i].y = GetRandomValue(0, HEIGHT);
		npcs[i].speed = SPEED_FACTOR / (npcs[i].radius ? npcs[i].radius : 1);
		npcs[i].vx = GetRandomValue(-npcs[i].speed, npcs[i].speed);
		npcs[i].vy = GetRandomValue(-npcs[i].speed, npcs[i].speed);
		npcs[i].radius = GetRandomValue(5, 30);
		npcs[i].color = RED;
		npcs[i].active = true;
		CustomLog(LOG_DEBUG, npcs[i].name);
	}
}

void DrawNpcs()
{
	for (int i = 0; i < NUM_NPCS; i++)
	{
		if (npcs[i].active)
		{
			DrawCircle(npcs[i].x, npcs[i].y, npcs[i].radius, npcs[i].color);
			DrawText(npcs[i].name, npcs[i].x - 20, npcs[i].y - 10, 10, WHITE); // Adjusted text positioning and size
		}
	}
}

void UpdateNpcs()
{
	for (int i = 0; i < NUM_NPCS; i++)
	{
		if (npcs[i].active)
		{
			// Update positions based on velocity
			npcs[i].x += npcs[i].vx;
			npcs[i].y += npcs[i].vy;

			// Check for boundary collision and reverse velocity if necessary
			if (npcs[i].x <= npcs[i].radius) // Adjust if NPC hits the left boundary
			{
				npcs[i].x = npcs[i].radius; // Set position exactly on the boundary
				npcs[i].vx = -npcs[i].vx;	// Reverse velocity
			}
			else if (npcs[i].x >= WIDTH - npcs[i].radius) // Adjust if NPC hits the right boundary
			{
				npcs[i].x = WIDTH - npcs[i].radius; // Set position exactly on the boundary
				npcs[i].vx = -npcs[i].vx;			// Reverse velocity
			}

			if (npcs[i].y <= npcs[i].radius) // Adjust if NPC hits the top boundary
			{
				npcs[i].y = npcs[i].radius; // Set position exactly on the boundary
				npcs[i].vy = -npcs[i].vy;	// Reverse velocity
			}
			else if (npcs[i].y >= HEIGHT - npcs[i].radius) // Adjust if NPC hits the bottom boundary
			{
				npcs[i].y = HEIGHT - npcs[i].radius; // Set position exactly on the boundary
				npcs[i].vy = -npcs[i].vy;			 // Reverse velocity
			}
		}
	}
}

void UpdatePlayerPosition(Circle *player)
{
	if (IsKeyDown(KEY_D))
	{ // Move right
		player->x = (player->x + player->speed > WIDTH - player->radius) ? WIDTH - player->radius : player->x + player->speed;
	}
	if (IsKeyDown(KEY_A))
	{ // Move left
		player->x = (player->x - player->speed < player->radius) ? player->radius : player->x - player->speed;
	}
	if (IsKeyDown(KEY_W))
	{ // Move up
		player->y = (player->y - player->speed < player->radius) ? player->radius : player->y - player->speed;
	}
	if (IsKeyDown(KEY_S))
	{ // Move down
		player->y = (player->y + player->speed > HEIGHT - player->radius) ? HEIGHT - player->radius : player->y + player->speed;
	}
}

int CheckCollision(Circle c1, Circle c2)
{
	float distance = sqrt(pow(c2.x - c1.x, 2) + pow(c2.y - c1.y, 2));
	return distance < (c1.radius + c2.radius);
}

void ProcessCollisions(Circle *a, Circle *b)
{
	if (a->active && b->active && CheckCollision(*a, *b))
	{
		if (a->radius < b->radius)
		{
			// A is smaller, B consumes A
			b->radius = (int)(b->radius * 1.2);
			b->speed = (b->speed * 0.99);
			a->active = false; // Mark A as inactive
		}
		else if (b->radius < a->radius)
		{
			// B is smaller, A consumes B
			a->radius = (int)(a->radius * 1.2);
			a->speed = (a->speed * 0.99);
			b->active = false; // Mark B as inactive
		}
	}
}

bool IsGameOver(Circle npcs[], int numNpcs)
{
	for (int i = 0; i < numNpcs; i++)
	{
		if (npcs[i].active) // If any NPC is still active, the game is not over
		{
			return false;
		}
	}
	return true; // All NPCs are inactive, game is over
}

int main()
{
	// window dimensions
	InitWindow(WIDTH, HEIGHT, "AGARio");
	InitNpcs();

	// circle coordinates
	Circle player = {"Player",
					 WIDTH / 2,
					 HEIGHT / 2,
					 0,
					 0,
					 25,
					 10,
					 BLUE,
					 true};

	SetTargetFPS(60);
	while (!WindowShouldClose() && player.active)
	{
		BeginDrawing();
		ClearBackground(BLACK);

		// Update player position based on user input
		UpdatePlayerPosition(&player);

		// Process collisions with NPCs
		for (int i = 0; i < NUM_NPCS; i++)
		{
			if (npcs[i].radius > 0)
				ProcessCollisions(&player, &npcs[i]);
			for (int j = 0; j < NUM_NPCS; j++)
			{
				if (i != j && npcs[j].radius > 0) // Skip the collision check with itself
					ProcessCollisions(&npcs[i], &npcs[j]);
			}
		}

		// Update NPCs
		UpdateNpcs();

		// Draw player and NPCs
		if (player.radius > 0)
		{ // Only draw player if still "alive"
			DrawCircle(player.x, player.y, player.radius, player.color);
			DrawText(player.name, player.x, player.y, 20, WHITE);
		}
		DrawNpcs();

		EndDrawing();

		// Check if the game is over
		if (IsGameOver(npcs, NUM_NPCS))
		{
			DrawText("Game Over", WIDTH / 2 - MeasureText("Game Over", 20) / 2, HEIGHT / 2 - 10, 20, WHITE);
			EndDrawing(); // Update the screen with "Game Over" message
			break;		  // Break out of the loop to end the game
		}
	}

	CloseWindow();

	return 0;
}
