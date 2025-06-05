#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#define FPGA_DOT_DEVICE "/dev/fpga_dot"

#define MAX_BUTTON 13

unsigned char quit = 0;

void user_signal1(int sig) 
{
	quit = 1;
}

int RandomValue()
{
	srand(time(0));
	int random = rand() % 6;
		
	int pownum = 1;
	
	for(int j = 0; j < random + 1; j++)
		pownum = pownum * 2;
		
	return pownum;
}

void make_obstacle()
{
	
}

int main(void)
{
	unsigned char push_sw_buff[MAX_BUTTON];
	int dev_push_switch = open("/dev/fpga_push_switch", O_RDWR);
	int buff_size;

	if (dev_push_switch<0){
		printf("Device Open Error\n");
		close(dev_push_switch);
		return -1;
	}


	unsigned char Player[1][10] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08};
	int dev_dot = open(FPGA_DOT_DEVICE, O_WRONLY);

	if (dev_dot<0) {
		printf("Device open error : %s\n",FPGA_DOT_DEVICE);
		exit(1);
	}

	int str_size;
	str_size=sizeof(Player[0]);
	write(dev_dot,Player[0],str_size);

	(void)signal(SIGINT, user_signal1);

	buff_size=sizeof(push_sw_buff);
	int i = 0;
	
	Player[0][0] = make_obstacle();		
	write(dev_dot,Player[0],str_size);

	while(!quit)
	{
		usleep(400000);
		if(i < 9){
			if((i+1) != 9)
				Player[0][i+1] = Player[0][i];
			Player[0][i] = 0x00;
			write(dev_dot,Player[0],str_size);
			i++;
			if(i == 8){
				Player[0][i] = 0x00;
				Player[0][0] = RandomValue();
				i = 0;
				write(dev_dot,Player[0],str_size);
			}
		}
		read(dev_push_switch, &push_sw_buff, buff_size);
		if(push_sw_buff[0] == 1)
		{
			Player[0][9] = Player[0][9] << 1;
			write(dev_dot,Player[0],str_size);
		}
		else if(push_sw_buff[2] == 1)
		{
			Player[0][9] = Player[0][9] >> 1;
			write(dev_dot,Player[0],str_size);
		}
	}
	close(dev_dot);
	close(dev_push_switch);
	
	return 0;
	
}
