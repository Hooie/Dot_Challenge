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

#define DOT_ROWS 10
#define DOT_COLS 7
#define BEEP_DURATION_US 200000 // 0.2초
int TrapUsed[DOT_ROWS][DOT_COLS] = { 0 };

unsigned char Maze[DOT_ROWS] = {
    0b1011101,  // 0
    0b1000001,  // 1 - trap at (1,5)
    0b1110101,  // 2
    0b1010111,  // 3
    0b1000101,  // 4
    0b1010001,  // 5
    0b1011011,  // 6
    0b1000011,  // 7
    0b1001001,  // 8
    0b1011011   // 9 - trap at (9,1)
};

#define NUM_TRAPS 2
int TrapRow[NUM_TRAPS] = { 1, 9 };
int TrapCol[NUM_TRAPS] = { 5, 1 };

int PlayerRow = 0, PlayerCol = 0;
unsigned char push_sw_buff[9];
int dev_dot, dev_push_switch, dev_buzzer;
int blink_state = 0; // 0 = OFF, 1 = ON

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
    printf("Trap triggered at (%d,%d)! Executing dot_challenge...\n", PlayerRow, PlayerCol);
    
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
            if (exit_code == 0) {
                printf("Survived dot_challenge. Returning to maze.\n");
            }
            else {
                printf("Failed in dot_challenge. Exiting game.\n");
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

    if (push_sw_buff[5] == 1 && PlayerCol < DOT_COLS - 1) { // →
        if (((Maze[PlayerRow] >> (6 - (PlayerCol + 1))) & 1) == 0) {
            PlayerCol++;
            moved = 1;
        }
    }
    else if (push_sw_buff[3] == 1 && PlayerCol > 0) { // ←
        if (((Maze[PlayerRow] >> (6 - (PlayerCol - 1))) & 1) == 0) {
            PlayerCol--;
            moved = 1;
        }
    }
    else if (push_sw_buff[1] == 1 && PlayerRow > 0) { // ↑
        if (((Maze[PlayerRow - 1] >> (6 - PlayerCol)) & 1) == 0) {
            PlayerRow--;
            moved = 1;
        }
    }
    else if (push_sw_buff[7] == 1 && PlayerRow < DOT_ROWS - 1) { // ↓
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

    if (IsTrap(PlayerRow, PlayerCol)) {
        TrapUsed[PlayerRow][PlayerCol] = 1; // 사용 처리
        HandleTrap();
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
        usleep(500000); // 0.5초마다 깜빡임 갱신
    }

    close(dev_dot);
    close(dev_push_switch);
    close(dev_buzzer);
    return 0;
}
