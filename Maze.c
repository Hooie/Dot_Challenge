#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#define DOT_DEVICE "/dev/fpga_dot"
#define SW_DEVICE "/dev/fpga_push_switch"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define TEXT_LCD_DEVICE "/dev/fpga_text_lcd"

#define DOT_ROWS 10
#define DOT_COLS 7
#define BEEP_DURATION_US 200000

int TrapUsed[DOT_ROWS][DOT_COLS] = { 0 };
unsigned char Maze[DOT_ROWS] = {
    0b1011101,
    0b1000001,
    0b1110101,
    0b1010111,
    0b1000101,
    0b1010001,
    0b1011011,
    0b1000011,
    0b1001001,
    0b1011011
};

#define NUM_TRAPS 2
int TrapRow[NUM_TRAPS] = { 1, 9 };
int TrapCol[NUM_TRAPS] = { 5, 1 };

int PlayerRow = 0, PlayerCol = 0;
unsigned char push_sw_buff[9];
int dev_dot, dev_push_switch, dev_buzzer, dev_text_lcd;
int blink_state = 0;

int IsTrap(int row, int col) {
    for (int i = 0; i < NUM_TRAPS; i++) {
        if (TrapRow[i] == row && TrapCol[i] == col && TrapUsed[row][col] == 0)
            return 1;
    }
    return 0;
}

void InitPlayerSpawnPosition() {
    for (int row = 0; row < DOT_ROWS; row++) {
        for (int col = 0; col < DOT_COLS; col++) {
            if (((Maze[row] >> (6 - col)) & 1) == 0) {
                PlayerRow = row;
                PlayerCol = col;
                return;
            }
        }
    }
    PlayerRow = 0;
    PlayerCol = 0;
}

void UpdateDisplay() {
    unsigned char display[DOT_ROWS];
    memcpy(display, Maze, sizeof(Maze));

    if (blink_state) {
        display[PlayerRow] |= (1 << (6 - PlayerCol));
    }
    else {
        display[PlayerRow] &= ~(1 << (6 - PlayerCol));
    }

    write(dev_dot, display, sizeof(display));
    blink_state = !blink_state;
}

void HandleTrap() {
    dev_text_lcd = open(TEXT_LCD_DEVICE, O_WRONLY);
    if (dev_text_lcd >= 0) {
        write(dev_text_lcd, "Trap Triggered !!               ", 32);
        close(dev_text_lcd);
    }

    close(dev_dot);
    close(dev_buzzer);
    close(dev_push_switch);

    pid_t pid = fork();
    if (pid == 0) {
        execl("./dot_challenge", "./dot_challenge", NULL);
        perror("execl failed");
        exit(1);
    }
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);

        dev_dot = open(DOT_DEVICE, O_WRONLY);
        dev_push_switch = open(SW_DEVICE, O_RDONLY);
        dev_buzzer = open(BUZZER_DEVICE, O_WRONLY);

        if (dev_dot < 0 || dev_push_switch < 0 || dev_buzzer < 0) {
            perror("device open error");
            exit(1);
        }

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                exit(0);
            }
        }
    }
    else {
        perror("fork failed");
        exit(1);
    }
}

void HandleInput() {
    read(dev_push_switch, &push_sw_buff, sizeof(push_sw_buff));
    int moved = 0;

    if (push_sw_buff[5] == 1 && PlayerCol < DOT_COLS - 1) {
        if (((Maze[PlayerRow] >> (6 - (PlayerCol + 1))) & 1) == 0) {
            PlayerCol++;
            moved = 1;
        }
    }
    else if (push_sw_buff[3] == 1 && PlayerCol > 0) {
        if (((Maze[PlayerRow] >> (6 - (PlayerCol - 1))) & 1) == 0) {
            PlayerCol--;
            moved = 1;
        }
    }
    else if (push_sw_buff[1] == 1 && PlayerRow > 0) {
        if (((Maze[PlayerRow - 1] >> (6 - PlayerCol)) & 1) == 0) {
            PlayerRow--;
            moved = 1;
        }
    }
    else if (push_sw_buff[7] == 1 && PlayerRow < DOT_ROWS - 1) {
        if (((Maze[PlayerRow + 1] >> (6 - PlayerCol)) & 1) == 0) {
            PlayerRow++;
            moved = 1;
        }
    }

    if (moved) {
        unsigned char on = 1;
        write(dev_buzzer, &on, 1);
        usleep(BEEP_DURATION_US);
        unsigned char off = 0;
        write(dev_buzzer, &off, 1);
    }

    // 트랩 감지
    if (IsTrap(PlayerRow, PlayerCol)) {
        TrapUsed[PlayerRow][PlayerCol] = 1;
        HandleTrap();
        return;
    }

    // 보스 진입
    if (PlayerRow == 9 && PlayerCol == 4) {
        dev_text_lcd = open(TEXT_LCD_DEVICE, O_WRONLY);
        if (dev_text_lcd >= 0) {
            write(dev_text_lcd, "Boss Stage Start!!              ", 32);
            close(dev_text_lcd);
        }

        close(dev_dot);
        close(dev_buzzer);
        close(dev_push_switch);

        pid_t pid = fork();
        if (pid == 0) {
            execl("./Boss", "./Boss", NULL);
            perror("execl failed");
            exit(1);
        }
        else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 0) {
                    exit(0);
                }
                else {
                    exit(0);
                }
            }
        }
        else {
            perror("fork failed");
            exit(1);
        }
    }
}

int main(void) {
    dev_dot = open(DOT_DEVICE, O_WRONLY);
    dev_push_switch = open(SW_DEVICE, O_RDONLY);
    dev_buzzer = open(BUZZER_DEVICE, O_WRONLY);

    if (dev_dot < 0 || dev_push_switch < 0 || dev_buzzer < 0) {
        perror("device open error");
        return 1;
    }

    InitPlayerSpawnPosition();

    while (1) {
        HandleInput();
        UpdateDisplay();
        usleep(500000);
    }

    close(dev_dot);
    close(dev_push_switch);
    close(dev_buzzer);
    return 0;
}
