#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define TEXT_LCD_DEVICE "/dev/fpga_text_lcd"
#define PUSH_SWITCH_DEVICE "/dev/fpga_push_switch"

int dev_lcd, dev_sw;
unsigned char push_sw_buff[9];

int any_button_pressed(unsigned char* buf) {
    for (int i = 0; i < 9; ++i) {
        if (buf[i] == 1) return 1;
    }
    return 0;
}

void ClearLCD() {
    const char* clear = "                                "; // 32 spaces
    write(dev_lcd, clear, 32);
}


void wait_button() {
    // 버튼이 모두 떼어질 때까지 기다림
    do {
        read(dev_sw, &push_sw_buff, sizeof(push_sw_buff));
        usleep(100000);
    } while (any_button_pressed(push_sw_buff));

    // 버튼이 새롭게 눌릴 때까지 기다림
    while (1) {
        read(dev_sw, &push_sw_buff, sizeof(push_sw_buff));
        if (any_button_pressed(push_sw_buff)) break;
        usleep(100000);
    }
}


int main() {
    dev_lcd = open(TEXT_LCD_DEVICE, O_WRONLY);
    dev_sw = open(PUSH_SWITCH_DEVICE, O_RDONLY);
    if (dev_lcd < 0 || dev_sw < 0) {
        perror("Device open failed");
        return -1;
    }

    // 시작 화면 출력
    ClearLCD();
    write(dev_lcd, "   Dot RPG      Press Any Key...", 32);
    wait_button();

    // 스크립트 출력 1
    ClearLCD();
    write(dev_lcd, "???: My town has been           ", 32);
    wait_button();

    // 스크립트 출력 2
    ClearLCD();
    write(dev_lcd, "taken over by a monster.        ", 32);
    wait_button();

    // 스크립트 출력 3
    ClearLCD();
    write(dev_lcd, "???: Please help us...          ", 32);
    wait_button();

    // 미로 시작 메시지
    ClearLCD();
    write(dev_lcd, "Escape the Maze !!              ", 32);
    usleep(1000000); // 1초 대기

    close(dev_lcd);
    close(dev_sw);

    // Miro 실행
    execl("./Miro", "./Miro", NULL);
    perror("Failed to launch Miro");
    return 1;
}
