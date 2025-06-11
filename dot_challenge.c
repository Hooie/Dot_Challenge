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
#define BEEP_DURATION_US 200000  // Buzzer beep duration (200ms)

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
} Obstacle;

Obstacle obstacles[MAX_OBSTACLES];

// Interrupt handler for clean exit
void user_signal1(int sig) { quit = 1; }

// Initialize all required devices
int InitDevices() {
    dev_push_switch = open("/dev/fpga_push_switch", O_RDWR);
    dev_dot = open(FPGA_DOT_DEVICE, O_WRONLY);
    dev_fnd = open(FND_FND_DEVICE, O_RDWR);
    dev_text_lcd = open(FPGA_TEXT_LCD_DEVICE, O_WRONLY);
    dev_buzzer = open(BUZZER_DEVICE, O_RDWR);
    return (dev_push_switch < 0 || dev_dot < 0 || dev_fnd < 0 || dev_text_lcd < 0 || dev_buzzer < 0)
        ? -1 : 0;
}

// Close all open devices
void CleanupDevices() {
    close(dev_dot);
    close(dev_fnd);
    close(dev_text_lcd);
    close(dev_buzzer);
    close(dev_push_switch);
}

// Convert integer score to 4-digit array for FND
void FormatFND(int value, unsigned char data[4]) {
    for (int i = 3; i >= 0; --i) {
        data[i] = value % 10;
        value /= 10;
    }
}

// Update FND display with current score
void UpdateFND() {
    unsigned char fnd_data[4];
    FormatFND(score, fnd_data);
    write(dev_fnd, &fnd_data, 4);
}

// Initialize obstacle array
void InitObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; ++i)
        obstacles[i].active = 0;
}

// Activate a new obstacle in a random column
void AddRandomObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!obstacles[i].active) {
            obstacles[i].row = 0;
            obstacles[i].col = rand() % DOT_COLS;
            obstacles[i].active = 1;
            break;
        }
    }
}

// Move obstacles downward and deactivate if out of bounds
void UpdateObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (obstacles[i].active) {
            obstacles[i].row += 1;
            if (obstacles[i].row >= DOT_ROWS - 1)
                obstacles[i].active = 0;
        }
    }
}

// Render obstacle positions into Player buffer (excluding last row)
void RenderObstacles() {
    for (int r = 0; r < DOT_ROWS - 1; ++r)
        Player[0][r] = 0x00;
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (obstacles[i].active) {
            int row1 = obstacles[i].row;
            int row2 = row1 + 1;
            if (row1 < DOT_ROWS - 1)
                Player[0][row1] |= (1 << obstacles[i].col);
            if (row2 < DOT_ROWS - 1)
                Player[0][row2] |= (1 << obstacles[i].col);
        }
    }
}

// Get the column index of the player
int GetPlayerCol(unsigned char playerByte) {
    for (int i = 0; i < DOT_COLS; ++i)
        if ((playerByte >> i) & 0x01)
            return i;
    return -1;
}

// Check collision between player and obstacles
int CheckCollision() {
    int col = GetPlayerCol(Player[0][DOT_ROWS - 1]);
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (obstacles[i].active) {
            int bottom = obstacles[i].row + 1;
            if (bottom >= DOT_ROWS - 1 && obstacles[i].col == col)
                return 1;
        }
    }
    return 0;
}

// Handle player input and beep buzzer on valid movement
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

// Display level selection and configure difficulty parameters
void SelectLevel() {
    const char* level[] = { " Easy ", "Normal", " Hard ", "Start!" };
    int level_count = 0;
    write(dev_text_lcd, level[level_count], strlen(level[level_count]));
    while (!quit) {
        usleep(400000);
        HandleInput();  // reuse input for level nav
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

// Main game loop with separate timers for input and game logic
void GameLoop() {
    InitObstacles();
    score = 0;
    Player[0][DOT_ROWS - 1] = 0x08;  // initial position center
    int input_tick = 0, game_tick = 0, tick = 0;
    while (!quit) {
        usleep(10000);  // 10ms frame
        input_tick += 10;
        game_tick += 10;
        if (input_tick >= INPUT_INTERVAL_MS) {
            HandleInput(); input_tick = 0;
        }
        if (game_tick >= game_speed_ms) {
            UpdateObstacles();
            if (tick % spawn_interval_tick == 0) {
                for (int i = 0; i < max_spawn_count; ++i)
                    AddRandomObstacle();
            }
            RenderObstacles();
            if (CheckCollision()) {
                printf("Collision! Game Over.\n");
                // Clear dot matrix
                unsigned char clear[DOT_ROWS] = { 0 };
                write(dev_dot, clear, sizeof(clear));
                // Play 8-beat buzzer rhythm
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
