#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define FPGA_DOT_DEVICE "/dev/fpga_dot"
#define FPGA_TEXT_LCD_DEVICE "/dev/fpga_text_lcd"
#define FND_FND_DEVICE "/dev/fpga_fnd"
#define MAX_BUTTON 13
#define MAX_OBSTACLES 10
#define DOT_COLS 8
#define DOT_ROWS 10

unsigned char quit = 0;
unsigned char Player[1][DOT_ROWS];
int dev_sw, dev_dot, dev_fnd, dev_lcd, score;

typedef struct { int row, col, active; } Obstacle;
Obstacle obs[MAX_OBSTACLES];

void user_signal(int s) { quit = 1; }

int InitDevices()
{
    dev_sw = open("/dev/fpga_push_switch", O_RDWR);
    dev_dot = open(FPGA_DOT_DEVICE, O_WRONLY);
    dev_fnd = open(FND_FND_DEVICE, O_RDWR);
    dev_lcd = open(FPGA_TEXT_LCD_DEVICE, O_WRONLY);
    return (dev_sw < 0 || dev_dot < 0 || dev_fnd < 0 || dev_lcd < 0) ? -1 : 0;
}

void Cleanup()
{
    close(dev_sw);
    close(dev_dot);
    close(dev_fnd);
    close(dev_lcd);
}

void InitObstacles()
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
        obs[i].active = 0;
}

void AddObstacle()
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (!obs[i].active)
        {
            obs[i].row = 0;
            obs[i].col = rand() % DOT_COLS;
            obs[i].active = 1;
            break;
        }
    }
}

void UpdateObstacles()
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
        if (obs[i].active && ++obs[i].row >= DOT_ROWS - 1)
            obs[i].active = 0;
}

void RenderObstacles()
{
    memset(Player[0], 0, DOT_ROWS - 1);
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (obs[i].active)
        {
            int r1 = obs[i].row;
            int r2 = r1 + 1;
            if (r1 < DOT_ROWS - 1)
                Player[0][r1] |= 1 << obs[i].col;
            if (r2 < DOT_ROWS - 1)
                Player[0][r2] |= 1 << obs[i].col;
        }
    }
}

int GetPlayerCol(unsigned char b)
{
    for (int i = 0; i < 8; i++)
        if ((b >> i) & 1)
            return i;
    return -1;
}

int CheckCollision()
{
    int c = GetPlayerCol(Player[0][DOT_ROWS - 1]);
    for (int i = 0; i < MAX_OBSTACLES; i++)
        if (obs[i].active && obs[i].row + 1 >= DOT_ROWS - 1 && obs[i].col == c)
            return 1;
    return 0;
}

void FormatFND(int v, unsigned char d[4])
{
    for (int i = 3; i >= 0; i--)
        d[i] = v % 10, v /= 10;
    write(dev_fnd, d, 4);
}

void HandleInput()
{
    unsigned char sb[MAX_BUTTON];
    read(dev_sw, sb, MAX_BUTTON);
    if (sb[0] == 1 && (Player[0][DOT_ROWS - 1] & 0x80) == 0)
        Player[0][DOT_ROWS - 1] <<= 1;
    else if (sb[2] == 1 && (Player[0][DOT_ROWS - 1] & 0x01) == 0)
        Player[0][DOT_ROWS - 1] >>= 1;
}

void GameLoop()
{
    InitObstacles();
    score = 0;
    Player[0][DOT_ROWS - 1] = 0x08;

    int it = 0, gt = 0, tick = 0;
    while (!quit)
    {
        usleep(10000); it += 10; gt += 10;
        if (it >= 50) { HandleInput(); it = 0; }
        if (gt >= 1000)
        {
            UpdateObstacles();
            if (tick++ % 2 == 0) AddObstacle();
            RenderObstacles();
            if (CheckCollision()) { printf("Game Over\n"); break; }
            unsigned char fd[4] = { 0 };
            FormatFND(++score, fd);
            write(dev_dot, Player[0], DOT_ROWS);
            gt = 0;
        }
    }
}

int main(void)
{
    signal(SIGINT, user_signal);
    srand(time(NULL));
    if (InitDevices() < 0) return -1;
    GameLoop();
    Cleanup();
    return 0;
}