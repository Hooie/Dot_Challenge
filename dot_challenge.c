#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>

#define FPGA_DOT_DEVICE "/dev/fpga_dot"
#define FPGA_TEXT_LCD_DEVICE "/dev/fpga_text_lcd"
#define FND_FND_DEVICE "/dev/fpga_fnd"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define MAX_BUTTON 13
#define MAX_OBSTACLES 10
#define DOT_COLS 8
#define DOT_ROWS 10
#define INPUT_INTERVAL_MS 50
#define BEEP_DURATION_US 200000

unsigned char quit = 0;
unsigned char Player[1][DOT_ROWS] = { 0 };
unsigned char push_sw_buff[MAX_BUTTON];
int dev_push_switch, dev_dot, dev_fnd, dev_text_lcd, dev_buzzer;
int score = 0;

int game_speed_ms = 1000;
int spawn_interval_tick = 2;
int max_spawn_count = 1;

typedef struct {
    int row;
    int col;
    int active;
    int direction; // 0: 수직, 1: 수평 왼->오, 2: 수평 오->왼
} Obstacle;

Obstacle verticalObstacles[MAX_OBSTACLES];
Obstacle leftObstacles[MAX_OBSTACLES];
Obstacle rightObstacles[MAX_OBSTACLES];

void user_signal1(int sig) { quit = 1; }

int InitDevices() {
    dev_push_switch = open("/dev/fpga_push_switch", O_RDWR);
    dev_dot = open(FPGA_DOT_DEVICE, O_WRONLY);
    dev_fnd = open(FND_FND_DEVICE, O_RDWR);
    dev_text_lcd = open(FPGA_TEXT_LCD_DEVICE, O_WRONLY);
    dev_buzzer = open(BUZZER_DEVICE, O_RDWR);
    return (dev_push_switch < 0 || dev_dot < 0 || dev_fnd < 0 || dev_text_lcd < 0 || dev_buzzer < 0)
        ? -1 : 0;
}

void CleanupDevices() {
    close(dev_dot);
    close(dev_fnd);
    close(dev_text_lcd);
    close(dev_buzzer);
    close(dev_push_switch);
}

void FormatFND(int value, unsigned char data[4]) {
    for (int i = 3; i >= 0; --i) {
        data[i] = value % 10;
        value /= 10;
    }
}

void UpdateFND() {
    unsigned char fnd_data[4];
    FormatFND(score, fnd_data);
    write(dev_fnd, &fnd_data, 4);
}

void InitObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        verticalObstacles[i].active = 0;
        leftObstacles[i].active = 0;
        rightObstacles[i].active = 0;
    }
}

void AddObstacle(Obstacle* array, int direction) {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!array[i].active) {
            array[i].row = rand() % (DOT_ROWS - 1);
            array[i].col = (direction == 1) ? 0 : DOT_COLS - 1;
            array[i].active = 1;
            array[i].direction = direction;
            return;
        }
    }
}

void AddVerticalObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!verticalObstacles[i].active) {
            verticalObstacles[i].row = 0;
            verticalObstacles[i].col = rand() % DOT_COLS;
            verticalObstacles[i].active = 1;
            verticalObstacles[i].direction = 0;
            return;
        }
    }
}

void UpdateObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (verticalObstacles[i].active) {
            verticalObstacles[i].row += 1;
            if (verticalObstacles[i].row >= DOT_ROWS - 1)
                verticalObstacles[i].active = 0;
        }
        if (leftObstacles[i].active) {
            leftObstacles[i].col += 1;
            if (leftObstacles[i].col >= DOT_COLS)
                leftObstacles[i].active = 0;
        }
        if (rightObstacles[i].active) {
            rightObstacles[i].col -= 1;
            if (rightObstacles[i].col < 0)
                rightObstacles[i].active = 0;
        }
    }
}

void RenderObstacles() {
    for (int r = 0; r < DOT_ROWS - 1; ++r)
        Player[0][r] = 0x00;

    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (verticalObstacles[i].active) {
            int r1 = verticalObstacles[i].row;
            int r2 = r1 + 1;
            if (r1 < DOT_ROWS - 1)
                Player[0][r1] |= (1 << verticalObstacles[i].col);
            if (r2 < DOT_ROWS - 1)
                Player[0][r2] |= (1 << verticalObstacles[i].col);
        }
        if (leftObstacles[i].active) {
            int c1 = leftObstacles[i].col;
            int c2 = c1 + 1;
            int row = leftObstacles[i].row;
            if (c1 < DOT_COLS)
                Player[0][row] |= (1 << c1);
            if (c2 < DOT_COLS)
                Player[0][row] |= (1 << c2);
        }
        if (rightObstacles[i].active) {
            int c1 = rightObstacles[i].col;
            int c2 = c1 - 1;
            int row = rightObstacles[i].row;
            if (c1 >= 0)
                Player[0][row] |= (1 << c1);
            if (c2 >= 0)
                Player[0][row] |= (1 << c2);
        }
    }
}

int GetPlayerCol(unsigned char playerByte) {
    for (int i = 0; i < DOT_COLS; ++i)
        if ((playerByte >> i) & 0x01)
            return i;
    return -1;
}

int CheckCollision() {
    int col = GetPlayerCol(Player[0][DOT_ROWS - 1]);
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if ((verticalObstacles[i].active && verticalObstacles[i].row + 1 == DOT_ROWS - 1 && verticalObstacles[i].col == col) ||
            (leftObstacles[i].active && leftObstacles[i].row == DOT_ROWS - 1 && leftObstacles[i].col == col) ||
            (rightObstacles[i].active && rightObstacles[i].row == DOT_ROWS - 1 && rightObstacles[i].col == col)) {
            return 1;
        }
    }
    return 0;
}

void HandleInput() {
    read(dev_push_switch, &push_sw_buff, sizeof(push_sw_buff));
    unsigned char* playerRow = &Player[0][DOT_ROWS - 1];
    int moved = 0;
    if (push_sw_buff[0] == 1 && (*playerRow & 0x80) == 0) {
        *playerRow <<= 1;
        moved = 1;
    }
    else if (push_sw_buff[2] == 1 && (*playerRow & 0x01) == 0) {
        *playerRow >>= 1;
        moved = 1;
    }
    if (moved) {
        unsigned char buzz_on = 1;
        write(dev_buzzer, &buzz_on, 1);
        usleep(BEEP_DURATION_US);
        unsigned char buzz_off = 0;
        write(dev_buzzer, &buzz_off, 1);
    }
}

void SelectLevel() {
    const char* level[] = { " Easy ", "Normal", " Hard ", "Start!" };
    int level_count = 0;
    write(dev_text_lcd, level[level_count], strlen(level[level_count]));
    while (!quit) {
        usleep(400000);
        HandleInput();
        if (push_sw_buff[1] == 1) {
            write(dev_text_lcd, level[3], strlen(level[3]));
            break;
        }
        else if (push_sw_buff[0] == 1 && level_count > 0) {
            level_count--;
            write(dev_text_lcd, level[level_count], strlen(level[level_count]));
        }
        else if (push_sw_buff[2] == 1 && level_count < 2) {
            level_count++;
            write(dev_text_lcd, level[level_count], strlen(level[level_count]));
        }
    }
    switch (level_count) {
    case 0: game_speed_ms = 1200; spawn_interval_tick = 3; max_spawn_count = 1; break;
    case 1: game_speed_ms = 800;  spawn_interval_tick = 2; max_spawn_count = 2; break;
    case 2: game_speed_ms = 500;  spawn_interval_tick = 1; max_spawn_count = 3; break;
    }
}

void GameLoop() {
    InitObstacles();
    score = 0;
    Player[0][DOT_ROWS - 1] = 0x08;
    int input_tick = 0, game_tick = 0, tick = 0;
    while (!quit) {
        usleep(10000);
        input_tick += 10;
        game_tick += 10;
        if (input_tick >= INPUT_INTERVAL_MS) {
            HandleInput(); input_tick = 0;
        }
        if (game_tick >= game_speed_ms) {
            UpdateObstacles();
            if (tick % spawn_interval_tick == 0) {
                for (int i = 0; i < max_spawn_count; ++i) {
                    AddVerticalObstacle();
                    AddObstacle(leftObstacles, 1);
                    AddObstacle(rightObstacles, 2);
                }
            }
            RenderObstacles();
            if (CheckCollision()) {
                printf("Collision! Game Over.\n");
                unsigned char clear[DOT_ROWS] = { 0 };
                write(dev_dot, clear, sizeof(clear));
                for (int i = 0; i < 8; ++i) {
                    unsigned char buzz_on = 1;
                    write(dev_buzzer, &buzz_on, 1);
                    usleep(BEEP_DURATION_US);
                    unsigned char buzz_off = 0;
                    write(dev_buzzer, &buzz_off, 1);
                    usleep(BEEP_DURATION_US);
                }
                break;
            }
            score++; UpdateFND(); game_tick = 0; tick++;
        }
        write(dev_dot, Player[0], sizeof(Player[0]));
    }
}

int main(void) {
    signal(SIGINT, user_signal1);
    srand(time(NULL));
    if (InitDevices() != 0) {
        printf("Device init failed\n");
        return -1;
    }
    SelectLevel();
    GameLoop();
    CleanupDevices();
    return 0;
}
