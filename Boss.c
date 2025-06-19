#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

#define DOT_DEVICE "/dev/fpga_dot"
#define SW_DEVICE "/dev/fpga_push_switch"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define LED_DEVICE "/dev/fpga_led"

#define DOT_ROWS 10
#define DOT_COLS 7

#define MAX_BULLETS 10
#define MAX_BOSS_BULLETS 20
#define BOSS_HP_MAX 4

int dev_dot, dev_sw, dev_buzzer, dev_led;
unsigned char push_sw_buff[9];
unsigned char screen[DOT_ROWS];

int player_row = DOT_ROWS - 1;
int player_col = 3;
int boss_col = 0;
int boss_dir = 1;
int boss_hp = BOSS_HP_MAX;
int player_hp = 4;
int boss_pattern_timer = 0;
int zigzag_count = 0;

typedef struct {
    int row, col, active, dx, dy;
} Bullet;

Bullet bullets[MAX_BULLETS];
Bullet boss_bullets[MAX_BOSS_BULLETS];

int input_cooldown = 0;

int InitDevices() {
    dev_dot = open(DOT_DEVICE, O_WRONLY);
    dev_sw = open(SW_DEVICE, O_RDONLY);
    dev_buzzer = open(BUZZER_DEVICE, O_WRONLY);
    dev_led = open(LED_DEVICE, O_RDWR);
    return (dev_dot < 0 || dev_sw < 0 || dev_buzzer < 0 || dev_led < 0) ? -1 : 0;
}

void CleanupDevices() {
    close(dev_dot); close(dev_sw); close(dev_buzzer); close(dev_led);
}

void Beep() {
    unsigned char on = 1, off = 0;
    write(dev_buzzer, &on, 1);
    usleep(100000);
    write(dev_buzzer, &off, 1);
}

void UpdateLED() {
    unsigned char led_val = 0;
    led_val |= ((0x0F << (8 - boss_hp)) & 0xF0); // Boss HP on upper 4 bits
    led_val |= ((0x0F >> (4 - player_hp)) & 0x0F); // Player HP on lower 4 bits
    write(dev_led, &led_val, 1);
}

void InitBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    for (int i = 0; i < MAX_BOSS_BULLETS; i++) boss_bullets[i].active = 0;
}

void FireBullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].row = player_row - 1;
            bullets[i].col = player_col;
            bullets[i].dx = 0;
            bullets[i].dy = -1;
            bullets[i].active = 1;
            Beep(); break;
        }
    }
}

void FireBossBullet(int row, int col, int dx, int dy) {
    for (int i = 0; i < MAX_BOSS_BULLETS; i++) {
        if (!boss_bullets[i].active) {
            boss_bullets[i].row = row;
            boss_bullets[i].col = col;
            boss_bullets[i].dx = dx;
            boss_bullets[i].dy = dy;
            boss_bullets[i].active = 1;
            break;
        }
    }
}

void FireBossSpecialPattern() {
    int pattern = rand() % 3;
    if (pattern == 0) {
        // Pattern 1: fire from left and right horizontally on all rows except boss row
        int gap_row1 = rand() % (DOT_ROWS - 3) + 1;
        int gap_row2 = gap_row1 + 1 + (rand() % (DOT_ROWS - gap_row1 - 2));
        for (int r = 1; r < DOT_ROWS; r++) {
            if (r == gap_row1 || r == gap_row1 + 1 || r == gap_row2 || r == gap_row2 + 1 || r == 0) continue; // skip gaps and boss row
            FireBossBullet(r, 0, 1, 0); // from left to right
            FireBossBullet(r, DOT_COLS - 1, -1, 0); // from right to left
        }
    }
    else if (pattern == 1) {
        // Pattern 2: fire 4~5 bullets from top
        int count = 4 + rand() % 2;
        for (int i = 0; i < count; i++) {
            int col = rand() % DOT_COLS;
            FireBossBullet(1, col, 0, 1);
        }
    }
    else if (pattern == 2) {
        // Pattern 3: 2x2 bullets from top
        int col = rand() % (DOT_COLS - 1);
        FireBossBullet(1, col, 0, 1);
        FireBossBullet(1, col + 1, 0, 1);
        FireBossBullet(2, col, 0, 1);
        FireBossBullet(2, col + 1, 0, 1);
    }
}

void MoveBullets() {
    static int boss_bullet_tick = 0;
    boss_bullet_tick++;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].row += bullets[i].dy;
            if (bullets[i].row < 0 || bullets[i].row >= DOT_ROWS) bullets[i].active = 0;
        }
    }

    if (boss_bullet_tick % 2 == 0) {
        for (int i = 0; i < MAX_BOSS_BULLETS; i++) {
            if (boss_bullets[i].active) {
                boss_bullets[i].row += boss_bullets[i].dy;
                boss_bullets[i].col += boss_bullets[i].dx;
                if (boss_bullets[i].row < 0 || boss_bullets[i].row >= DOT_ROWS ||
                    boss_bullets[i].col < 0 || boss_bullets[i].col >= DOT_COLS) {
                    boss_bullets[i].active = 0;
                }
            }
        }
    }
}

void MoveBoss() {
    boss_col += boss_dir;
    if (boss_col <= 0 || boss_col >= DOT_COLS - 1) boss_dir *= -1;
}

int CheckCollisions() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && bullets[i].row == 0 && bullets[i].col == boss_col) {
            bullets[i].active = 0;
            boss_hp--;
            UpdateLED();
            Beep();
            return (boss_hp <= 0) ? 1 : 0;
        }
    }
    for (int i = 0; i < MAX_BOSS_BULLETS; i++) {
        if (boss_bullets[i].active && boss_bullets[i].row == player_row && boss_bullets[i].col == player_col) {
            boss_bullets[i].active = 0;
            player_hp--;
            UpdateLED();
            Beep();
            if (player_hp <= 0) return -1;
        }
    }
    return 0;
}

void RenderScreen() {
    memset(screen, 0, sizeof(screen));
    screen[0] |= (1 << (6 - boss_col));
    screen[player_row] |= (1 << (6 - player_col));
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active)
            screen[bullets[i].row] |= (1 << (6 - bullets[i].col));
    }
    for (int i = 0; i < MAX_BOSS_BULLETS; i++) {
        if (boss_bullets[i].active)
            screen[boss_bullets[i].row] |= (1 << (6 - boss_bullets[i].col));
    }
    write(dev_dot, screen, sizeof(screen));
}

void HandleInput() {
    read(dev_sw, &push_sw_buff, sizeof(push_sw_buff));
    if (push_sw_buff[5] == 1 && player_col < DOT_COLS - 1)
        player_col++;
    else if (push_sw_buff[3] == 1 && player_col > 0)
        player_col--;
    else if (push_sw_buff[1] == 1 && player_row > 0)
        player_row--;
    else if (push_sw_buff[7] == 1 && player_row < DOT_ROWS - 1)
        player_row++;
    else if (push_sw_buff[10] == 1)
        FireBullet();
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
    usleep(10000000); // 1ÃÊ ´ë±â
}

int main() {
    if (InitDevices() < 0) {
        perror("Device open failed");
        return -1;
    }

    srand(time(NULL));
    InitBullets();
    UpdateLED();
    int tick = 0;

    while (1) {
        if (input_cooldown <= 0) {
            HandleInput();
            input_cooldown = 1;
        }
        else {
            input_cooldown--;
        }

        if (tick % 10 == 0) MoveBoss();
        if (tick % 20 == 0) FireBossBullet(1, boss_col, 0, 1);
        if (tick % 50 == 0) FireBossSpecialPattern();

        MoveBullets();

        int result = CheckCollisions();
        RenderScreen();

        if (result == 1) { printf("Victory!\n"); break; }
        if (result == -1) { printf("Defeat!\n"); DisplaySkull(); break; }

        usleep(100000);
        tick++;
    }

    CleanupDevices();
    return 0;
}
