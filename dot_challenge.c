#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define FPGA_DOT_DEVICE "/dev/fpga_dot"
#define FPGA_TEXT_LCD_DEVICE "/dev/fpga_text_lcd"
#define MAX_BUTTON 13
#define MAX_OBSTACLES 10
#define DOT_COLS 8
#define DOT_ROWS 10

unsigned char quit = 0;

void user_signal1(int sig) { quit = 1; }

typedef struct {
    int row;
    int col;
    int active;
} Obstacle;

Obstacle obstacles[MAX_OBSTACLES];

void InitObstacles()
{
    for (int i = 0; i < MAX_OBSTACLES; ++i)
        obstacles[i].active = 0;
}

void AddRandomObstacle()
{
    for (int i = 0; i < MAX_OBSTACLES; ++i)
    {
        if (!obstacles[i].active)
        {
            obstacles[i].row = 0;
            obstacles[i].col = rand() % DOT_COLS;
            obstacles[i].active = 1;
            break;
        }
    }
}

void UpdateObstacles()
{
    for (int i = 0; i < MAX_OBSTACLES; ++i)
    {
        if (obstacles[i].active)
        {
            obstacles[i].row++;
            if (obstacles[i].row >= DOT_ROWS - 1)
                obstacles[i].active = 0;
        }
    }
}

void RenderObstacles(unsigned char Player[1][DOT_ROWS])
{
    for (int r = 0; r < DOT_ROWS - 1; ++r)
        Player[0][r] = 0x00;

    for (int i = 0; i < MAX_OBSTACLES; ++i)
    {
        if (obstacles[i].active)
        {
            int r1 = obstacles[i].row;
            int r2 = r1 + 1;

            if (r1 < DOT_ROWS - 1)
                Player[0][r1] |= (1 << obstacles[i].col);
            if (r2 < DOT_ROWS - 1)
                Player[0][r2] |= (1 << obstacles[i].col);
        }
    }
}

int GetPlayerCol(unsigned char b)
{
    for (int i = 0; i < DOT_COLS; ++i)
        if ((b >> i) & 1)
            return i;
    return -1;
}

int CheckCollision(unsigned char playerByte)
{
    int pc = GetPlayerCol(playerByte);
    for (int i = 0; i < MAX_OBSTACLES; ++i)
        if (obstacles[i].active &&
            obstacles[i].row + 1 >= DOT_ROWS - 1 &&
            obstacles[i].col == pc)
            return 1;
    return 0;
}

void FormatFND(int v, unsigned char d[4])
{
    for (int i = 3; i >= 0; --i) { d[i] = v % 10; v /= 10; }
}

int main(void)
{
    signal(SIGINT, user_signal1);
    srand(time(NULL));

    int dev_sw = open("/dev/fpga_push_switch", O_RDWR);
    int dev_dot = open(FPGA_DOT_DEVICE, O_WRONLY);
    int dev_lcd = open(FPGA_TEXT_LCD_DEVICE, O_WRONLY);
    if (dev_sw < 0 || dev_dot < 0 || dev_fnd < 0 || dev_lcd < 0) return -1;

    unsigned char Player[1][DOT_ROWS] = { {0} };
    Player[0][DOT_ROWS - 1] = 0x08;  // start at center
    unsigned char swbuff[MAX_BUTTON];

    InitObstacles();
    int tick = 0, score = 0;
    while (!quit)
    {
        usleep(1000000);
        UpdateObstacles();
        if (tick % 2 == 0) AddRandomObstacle();
        RenderObstacles(Player);

        read(dev_sw, swbuff, sizeof(swbuff));
        if (swbuff[0] == 1 && (Player[0][DOT_ROWS - 1] & 0x80) == 0)
            Player[0][DOT_ROWS - 1] <<= 1;
        else if (swbuff[2] == 1 && (Player[0][DOT_ROWS - 1] & 0x01) == 0)
            Player[0][DOT_ROWS - 1] >>= 1;

        if (CheckCollision(Player[0][DOT_ROWS - 1]))
        {
            printf("Game Over\n");
            break;
        }

        write(dev_dot, Player[0], DOT_ROWS);
        tick++;
    }

    close(dev_sw);
    close(dev_dot);
    close(dev_lcd);
    return 0;
}