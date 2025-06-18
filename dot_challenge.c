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
#define BUZZER_TOGGLE_INTERVAL 100000
#define MAX_BUTTON 13
#define MAX_OBSTACLES 10
#define DOT_COLS 8
#define DOT_ROWS 10
#define INPUT_INTERVAL_MS 50
#define BEEP_DURATION_US 200000

unsigned char quit = 0;
unsigned char push_sw_buff[MAX_BUTTON];
int dev_push_switch, dev_dot, dev_fnd, dev_text_lcd, dev_buzzer;
int score = 0;
int game_time = 0; // ms
int mode = 0; // 0: 상하, 1: 좌우
int mode_time = 0;
int warning_state = 0;
int warning_timer = 0;

int game_speed_ms = 500;
int spawn_interval_tick = 1;
int max_spawn_count = 3;

unsigned char ObstacleBuffer[DOT_ROWS] = { 0 };
int PlayerCol = 3; // 5번째 열 (0-indexed)
int PlayerRow = 4; // 5번째 행 (0-indexed)

typedef struct {
    int row;
    int col;
    int active;
    int direction; // 0: 아래, 1: 위, 2: 오른쪽, 3: 왼쪽
} Obstacle;

Obstacle obstacles[MAX_OBSTACLES];

void user_signal1(int sig) { quit = 1; }

void PlayerEncounterMelody() {
    for (int i = 0; i < 10; ++i) {
        int on = 1;
        write(dev_buzzer, &on, 4);
        usleep(50000);
        int off = 0;
        write(dev_buzzer, &off, 4);
        usleep(50000);
    }
}

void DisplaySkull() {
    unsigned char skull[DOT_ROWS] = {
        0b00011100,
        0b00100010,
        0b01010101,
        0b01000001,
        0b01010101,
        0b01011001,
        0b00100010,
        0b00011100,
        0b00000000,
        0b00000000
    };
    write(dev_dot, skull, sizeof(skull));
    usleep(10000000); // 1초 대기
}

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
        obstacles[i].active = 0;
    }
}

void AddObstacle(int direction) {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!obstacles[i].active) {
            if (direction == 0) { // 상 -> 하
                obstacles[i].row = 0;
                obstacles[i].col = rand() % DOT_COLS;
            }
            else if (direction == 1) { // 하 -> 상
                obstacles[i].row = DOT_ROWS - 1;
                obstacles[i].col = rand() % DOT_COLS;
            }
            else if (direction == 2) { // 좌 -> 우
                obstacles[i].col = 0;
                obstacles[i].row = rand() % DOT_ROWS;
            }
            else if (direction == 3) { // 우 -> 좌
                obstacles[i].col = DOT_COLS - 1;
                obstacles[i].row = rand() % DOT_ROWS;
            }
            obstacles[i].active = 1;
            obstacles[i].direction = direction;
            return;
        }
    }
}

void UpdateObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (!obstacles[i].active) continue;
        switch (obstacles[i].direction) {
        case 0: obstacles[i].row += 1; break; // 아래로
        case 1: obstacles[i].row -= 1; break; // 위로
        case 2: obstacles[i].col += 1; break; // 오른쪽으로
        case 3: obstacles[i].col -= 1; break; // 왼쪽으로
        }
        if (obstacles[i].row < 0 || obstacles[i].row >= DOT_ROWS ||
            obstacles[i].col < 0 || obstacles[i].col >= DOT_COLS) {
            obstacles[i].active = 0;
        }
    }
}

void RenderObstacles() {
    memset(ObstacleBuffer, 0, sizeof(ObstacleBuffer));
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (obstacles[i].active) {
            ObstacleBuffer[obstacles[i].row] |= (1 << obstacles[i].col);
        }
    }
}

void RenderAll() {
    unsigned char frame[DOT_ROWS];
    memcpy(frame, ObstacleBuffer, sizeof(ObstacleBuffer));
    if (PlayerRow >= 0 && PlayerRow < DOT_ROWS)
        frame[PlayerRow] |= (1 << PlayerCol);
    write(dev_dot, frame, sizeof(frame));
}

int CheckCollision() {
    for (int i = 0; i < MAX_OBSTACLES; ++i) {
        if (obstacles[i].active &&
            obstacles[i].row == PlayerRow &&
            obstacles[i].col == PlayerCol) {
            return 1;
        }
    }
    return 0;
}

void HandleInput() {
    read(dev_push_switch, &push_sw_buff, sizeof(push_sw_buff));
    int moved = 0;
    if (mode == 0) { // 처음 5초: 좌우
        if (push_sw_buff[3] == 1 && PlayerCol < DOT_COLS - 1) {
            PlayerCol++;
            moved = 1;
        }
        else if (push_sw_buff[5] == 1 && PlayerCol > 0) {
            PlayerCol--;
            moved = 1;
        }
    }
    else if (mode == 1) { // 5초 후: 상하
        if (push_sw_buff[1] == 1 && PlayerRow > 0) {
            PlayerRow--;
            moved = 1;
        }
        else if (push_sw_buff[7] == 1 && PlayerRow < DOT_ROWS - 1) {
            PlayerRow++;
            moved = 1;
        }
    }
    if (moved) {
        unsigned char buzz_on = 1;
        write(dev_buzzer, &buzz_on, 1);
        usleep(BEEP_DURATION_US);
        unsigned char buzz_off = 0;
        write(dev_buzzer, &buzz_off, 1);
    }
}


void SpiralOpening() {
    PlayerEncounterMelody();
    unsigned char frame[DOT_ROWS] = { 0 };
    int visited[DOT_ROWS][DOT_COLS] = { 0 };
    int dr[4] = { 0, 1, 0, -1 };
    int dc[4] = { 1, 0, -1, 0 };
    int dir = 0, r = 0, c = 0;

    for (int i = 0; i < DOT_ROWS * DOT_COLS; ++i) {
        frame[r] |= (1 << c);
        write(dev_dot, frame, sizeof(frame));
        int on = 1; write(dev_buzzer, &on, 4); usleep(50000); int off = 0; write(dev_buzzer, &off, 4);
        visited[r][c] = 1;

        int nr = r + dr[dir];
        int nc = c + dc[dir];
        if (nr < 0 || nr >= DOT_ROWS || nc < 0 || nc >= DOT_COLS || visited[nr][nc]) {
            dir = (dir + 1) % 4;
            nr = r + dr[dir];
            nc = c + dc[dir];
        }
        r = nr; c = nc;
    }

    for (int i = 0; i < 4; ++i) {
        unsigned char clear[DOT_ROWS] = { 0 };
        write(dev_dot, clear, sizeof(clear));
        usleep(150000);
        write(dev_dot, frame, sizeof(frame));
        usleep(150000);
    }

    unsigned char clear[DOT_ROWS] = { 0 };
    write(dev_dot, clear, sizeof(clear));
}

void GameLoop() {
    SpiralOpening();
    InitObstacles();
    score = 0;
    PlayerCol = 3;
    PlayerRow = 4;
    int input_tick = 0, game_tick = 0, tick = 0;
    game_time = 0;

    while (!quit) {
        int warning_shown = 0;

        usleep(10000);
        input_tick += 10;
        game_tick += 10;
        game_time += 10;

        mode_time += 10;

        if (warning_state == 1) {
            warning_timer += 10;
            if ((warning_timer / 300) % 2 == 0) {
                unsigned char frame[DOT_ROWS] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
                write(dev_dot, frame, sizeof(frame));
            }
            else {
                unsigned char frame[DOT_ROWS] = { 0 };
                write(dev_dot, frame, sizeof(frame));
            }
            if (warning_timer >= 3000) {
                warning_state = 0;
                warning_timer = 0;
                write(dev_text_lcd, "                ", 16);
                InitObstacles();
                PlayerCol = 3; PlayerRow = 4;
                mode = (mode + 1) % 2;
                mode_time = 0;
            }
            continue;
        }
               // 5초마다 경고문 띄우기
        if (mode_time == 5000) {
            const char* warning_msg = "Warning: Shift!";
            write(dev_text_lcd, warning_msg, strlen(warning_msg));
            warning_state = 1;
            warning_timer = 0;
            continue;
        }

        if (input_tick >= INPUT_INTERVAL_MS) {
            HandleInput();
            input_tick = 0;
        }
        if (game_tick >= game_speed_ms) {
            UpdateObstacles();

            if (tick % spawn_interval_tick == 0) {
                for (int i = 0; i < max_spawn_count; ++i) {
                    if (mode == 0) {
                        AddObstacle(0); // 상 -> 하
                        AddObstacle(1); // 하 -> 상
                    }
                    else if (mode == 1) {
                        AddObstacle(2); // 좌 -> 우
                        AddObstacle(3); // 우 -> 좌
                    }
                }
            }

            RenderObstacles();
            RenderAll();

            if (CheckCollision()) {
                DisplaySkull();
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

            score++;
            UpdateFND();
            game_tick = 0;
            tick++;
        }
    }
}

int main(void) {
    signal(SIGINT, user_signal1);
    srand(time(NULL));
    if (InitDevices() != 0) {
        printf("Device init failed\n");
        return -1;
    }
    GameLoop();
    CleanupDevices();
    return 0;
}
