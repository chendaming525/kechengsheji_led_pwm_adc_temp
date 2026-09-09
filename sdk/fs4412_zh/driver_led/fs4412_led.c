#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "fs4412_led.h"

MODULE_LICENSE("Dual BSD/GPL"); //版权开源

#define LED_MA 500
#define LED_MI 0
#define LED_NUM 1

//寄存器地址
#define FS4412_GPF3CON	0x114001E0
#define FS4412_GPF3DAT	0x114001E4

#define FS4412_GPX1CON	0x11000C20
#define FS4412_GPX1DAT	0x11000C24

#define FS4412_GPX2CON	0x11000C40
#define FS4412_GPX2DAT	0x11000C44



/* 定义寄存器指针变量*/
static unsigned int *gpf3con;
static unsigned int *gpf3dat;

static unsigned int *gpx1con;
static unsigned int *gpx1dat;

static unsigned int *gpx2con;
static unsigned int *gpx2dat;


/* 定义结构体变量*/
struct cdev cdev;


/* 点亮LED灯*/
void fs4412_led_on(int nr)
{
	switch(nr) {
		case 1: 
			writel(readl(gpx2dat) | 1 << 7, gpx2dat);
			break;
		case 2: 
			writel(readl(gpx1dat) | 1 << 0, gpx1dat);
			break;
		case 3: 
			writel(readl(gpf3dat) | 1 << 4, gpf3dat);
			break;
		case 4: 
			writel(readl(gpf3dat) | 1 << 5, gpf3dat);
			break;
	}
}


/* 关闭LED灯*/
void fs4412_led_off(int nr)
{
	switch(nr) {
		case 1: 
			writel(readl(gpx2dat) & ~(1 << 7), gpx2dat);
			break;
		case 2: 
			writel(readl(gpx1dat) & ~(1 << 0), gpx1dat);
			break;
		case 3: 
			writel(readl(gpf3dat) & ~(1 << 4), gpf3dat);
			break;
		case 4: 
			writel(readl(gpf3dat) & ~(1 << 5), gpf3dat);
			break;
	}
}


/* 与应用程序对应，对应OPEN*/
static int s5pv210_led_open(struct inode *inode, struct file *file)
{
	return 0;
}


/* 与应用程序对应，对应退出（Ctrl+C）*/	
static int s5pv210_led_release(struct inode *inode, struct file *file)
{
	return 0;
}
	
	
/* 与应用程序对应，对应ioctl*/	
static long s5pv210_led_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int nr;

	if(copy_from_user((void *)&nr, (void *)arg, sizeof(nr)))
		return -EFAULT;

	if (nr < 1 || nr > 4)
		return -EINVAL;

	switch (cmd) {
		case LED_ON:
			fs4412_led_on(nr);
			break;
		case LED_OFF:
			fs4412_led_off(nr);
			break;
		default:
			printk("Invalid argument");
			return -EINVAL;
	}

	return 0;
}


/* I/O映射，物理地址映射为虚拟地址*/
int fs4412_led_ioremap(void)
{
	int ret;

	gpf3con = ioremap(FS4412_GPF3CON, 4);
	if (gpf3con == NULL) {
		printk("ioremap gpf3con\n");
		ret = -ENOMEM;
		return ret;
	}

	gpf3dat = ioremap(FS4412_GPF3DAT, 4);
	if (gpf3dat == NULL) {
		printk("ioremap gpx2dat\n");
		ret = -ENOMEM;
		return ret;
	}


	gpx1con = ioremap(FS4412_GPX1CON, 4);
	if (gpx1con == NULL) {
		printk("ioremap gpx2con\n");
		ret = -ENOMEM;
		return ret;
	}

	gpx1dat = ioremap(FS4412_GPX1DAT, 4);
	if (gpx1dat == NULL) {
		printk("ioremap gpx2dat\n");
		ret = -ENOMEM;
		return ret;
	}
	gpx2con = ioremap(FS4412_GPX2CON, 4);
	if (gpx2con == NULL) {
		printk("ioremap gpx2con\n");
		ret = -ENOMEM;
		return ret;
	}

	gpx2dat = ioremap(FS4412_GPX2DAT, 4);
	if (gpx2dat == NULL) {
		printk("ioremap gpx2dat\n");
		ret = -ENOMEM;
		return ret;
	}

	return 0;
}


/* 释放寄存器地址映射*/
void fs4412_led_iounmap(void)
{
	iounmap(gpf3con);
	iounmap(gpf3dat);
	iounmap(gpx1con);
	iounmap(gpx1dat);
	iounmap(gpx2con);
	iounmap(gpx2dat);
}


/* I/O初始化*/
void fs4412_led_io_init(void)
{

	writel((readl(gpf3con) & ~(0xff << 16)) | (0x11 << 16), gpf3con);
	writel(readl(gpx2dat) & ~(0x3<<4), gpf3dat);

	writel((readl(gpx1con) & ~(0xf << 0)) | (0x1 << 0), gpx1con);
	writel(readl(gpx1dat) & ~(0x1<<0), gpx1dat);

	writel((readl(gpx2con) & ~(0xf << 28)) | (0x1 << 28), gpx2con);
	writel(readl(gpx2dat) & ~(0x1<<7), gpx2dat);
}
	
	
	
/* 定位文件操作函数*/	
struct file_operations s5pv210_led_fops = {
	.owner = THIS_MODULE,
	.open = s5pv210_led_open,
	.release = s5pv210_led_release,
	.unlocked_ioctl = s5pv210_led_unlocked_ioctl,
};


/* 创建字符设备，进行点灯初始化*/
static int s5pv210_led_init(void)
{
	dev_t devno = MKDEV(LED_MA, LED_MI); //由主次设备号获得dev_t的宏
	int ret;

	ret = register_chrdev_region(devno, LED_NUM, "newled");//并为其分配设备号，为注册设备做准备
	if (ret < 0) {
		printk("register_chrdev_region\n");
		return ret;
	}

	cdev_init(&cdev, &s5pv210_led_fops);//初始化，建立cdev和file_operation 之间的连接
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, devno, LED_NUM);//注册设备
	if (ret < 0) {
		printk("cdev_add\n");
		goto err1;
	}

	ret = fs4412_led_ioremap(); //将控制LED灯的寄存器物理地址映射为虚拟地址
	if (ret < 0)
		goto err2;


	fs4412_led_io_init();

	printk("Led init\n");

	return 0;
err2:
	cdev_del(&cdev); //注销设备，对应cdev_add
err1:
	unregister_chrdev_region(devno, LED_NUM); //释放之前申请的设备号，对应register_chrdev_region
	return ret;
}


static void s5pv210_led_exit(void) //资源释放函数
{
	dev_t devno = MKDEV(LED_MA, LED_MI);

	fs4412_led_iounmap(); //取消地址映射，对应fs4412_led_init()中的fs4412_led_ioremap()
	cdev_del(&cdev); //注销设备  对应fs4412_led_init()中的cdev_add（）
	unregister_chrdev_region(devno, LED_NUM); //释放fs4412_led_init()中通过register_chrdev_region（）注册的设备号
	printk("Led exit\n");
}


module_init(s5pv210_led_init);
module_exit(s5pv210_led_exit);
