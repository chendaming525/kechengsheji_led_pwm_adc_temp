#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "pwm_music.h"

#include "driver_pwm/fs4412_pwm.h"
#include "driver_led/fs4412_led.h"

int fd;
int data;
float data1;
char flag;
int i = 0;
int n = 4;
int dev_fd;
int div1;
int pre = 255;
int fd_led;
int i0 = 1;
void buzz();
void led();
int main(int argc, const char *argv[])
{

	fd = open("/dev/adc", O_RDWR); // 打开通过mknod命令创建的设备文件
	if (fd < 0)
	{
		perror("open");
		exit(1);
	}

	dev_fd = open("/dev/pwm", O_RDWR | O_NONBLOCK);
	if (dev_fd == -1)
	{
		perror("open");
		exit(1);
	}
	ioctl(dev_fd, PWM_OFF);
	fd_led = open("/dev/led", O_RDWR);
	if (fd_led < 0)
	{
		perror("open");
		exit(1);
	}
	ioctl(dev_fd, SET_PRE, &pre);
	while (1)
	{
		// 应用程序通过read函数访问驱动的read接口读取ad转换后的值
		// 函数原型int read(int handle, void *buf, int count);
		// data用于存放转换后的数据
		read(fd, &data, sizeof(data));
		// 打印转换后的数据data
		printf("digital data is : %d:\n", data);
		// 把data转化为模拟电压值
		data1 = 1.8 * data / 4096;
		if (data1 >= 0.8)
		{
			buzz();
		}
		else if (data1 < 0.8)
		{
			ioctl(dev_fd, PWM_OFF);
			ioctl(fd_led, LED_OFF, &i0);
		}
		printf("analog data is : %0.4fV\n", data1);
		sleep(1);
	}
	return 0;
}
void buzz()
{
	ioctl(dev_fd, PWM_ON);
	for (i = 0; i < sizeof(MotherLoveMeOnceAgain) / sizeof(Note); i++)
	{
		led();
		read(fd, &data, sizeof(data));
		printf("digital data is : %d:\n", data);
		printf("analog data is : %0.4fV\n", data1);
		data1 = 1.8 * data / 4096;
		div1 = (PCLK / 256 / 4) / (MotherLoveMeOnceAgain[i].pitch);
		ioctl(dev_fd, SET_CNT, &div1);
		usleep(MotherLoveMeOnceAgain[i].dimation * 50);
		if (data1 < 0.8)
		{
			ioctl(dev_fd, PWM_OFF);
			break;
		}
	}
}
void led()
{
	ioctl(fd_led, LED_ON, &i0);	 // 请在此处添加第i个灯亮的代码，入口函数int ioctl(int fd,int cmd,char *argp);
	usleep(50000);				 // 在<unistd.h>头文件中声明usleep进程挂起，单位微秒
	ioctl(fd_led, LED_OFF, &i0); // 请在此处添加第i个灯亮的代码，入口函数int ioctl(int fd,int cmd,char *argp);
	usleep(50000);
	if (++i0 == 6)
		i0 = 1;
}
