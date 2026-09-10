#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "fs4412_pwm.h"

MODULE_LICENSE("GPL");

#define PWM_MA 501
#define PWM_MI 0
#define PWM_NUM 1

struct fs4412_pwm
{
	unsigned int *gpdcon;
	void __iomem *timer_base;
	struct cdev cdev;
};

static struct fs4412_pwm *pwm;

static int fs4412_pwm_open(struct inode *inode, struct file *file)
{
	writel((readl(pwm->gpdcon) & ~0xf) | 0x2, pwm->gpdcon);							// 设置GPD0CON控制寄存器[3:0]的值为0x2, 则TOUT0信号会输出PWM信号。
	writel(readl(pwm->timer_base + TCFG0) & ~0xff | 0xC7, pwm->timer_base + TCFG0); // 一级分频  分频值为199  即200分频   199的16进制为0xC7
	writel((readl(pwm->timer_base + TCFG1) & ~0xf) | 0x1, pwm->timer_base + TCFG1); // 二级分频，选分频值为1/2 即2分频
	writel(300, pwm->timer_base + TCNTB0);											// pwm初值为300
	writel(150, pwm->timer_base + TCMPB0);											// pwm电平翻转值为150
	writel((readl(pwm->timer_base + TCON) & ~0xf) | 0x2, pwm->timer_base + TCON);	// 定时器手动更新，即加载TCNTB0 和 TCMPB0的值
	return 0;

	return 0;
}

/*
static int fs4412_pwm_init(struct inode *inode, struct file *file)
{
	writel((readl(pwm->gpdcon) & ~0xf) | 0x2, pwm->gpdcon); //设置GPD0CON控制寄存器[3:0]的值为0x2, 则TOUT0信号会输出PWM信号。
	writel(readl(pwm->timer_base + TCFG0) & ~0xff | 0xC7, pwm->timer_base + TCFG0); //一级分频  分频值为199  即200分频   199的16进制为0xC7
	writel((readl(pwm->timer_base + TCFG1) & ~0xf) | 0x1, pwm->timer_base + TCFG1); //二级分频，选分频值为1/2 即2分频
	writel(300, pwm->timer_base + TCNTB0);  //pwm初值为300
	writel(150, pwm->timer_base + TCMPB0);  //pwm电平翻转值为150
	writel((readl(pwm->timer_base + TCON) & ~0xf) | 0x2, pwm->timer_base + TCON);  //定时器手动更新，即加载TCNTB0 和 TCMPB0的值
	return 0;
}
*/

static int fs4412_pwm_rlease(struct inode *inode, struct file *file)
{
	writel(readl(pwm->timer_base + TCON) & ~0xf, pwm->timer_base + TCON);
	return 0;
}

static long fs4412_pwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int data;

	if (_IOC_TYPE(cmd) != 'K')
		return -ENOTTY;

	if (_IOC_NR(cmd) > 3)
		return -ENOTTY;

	if (_IOC_DIR(cmd) == _IOC_WRITE)
		if (copy_from_user(&data, (void *)arg, sizeof(data)))
			return -EFAULT;

	switch (cmd)
	{
	case PWM_ON:
		// 设置寄存器TCON，使定时器开启，并启用自动重载
		writel((readl(pwm->timer_base + TCON) & ~0xf) | 0x9, pwm->timer_base + TCON);
		break;
	case PWM_OFF:
		// 设置寄存器TCON，使定时器关闭，低四位清零
		writel(readl(pwm->timer_base + TCON) & ~0xf, pwm->timer_base + TCON);
		break;
	case SET_PRE:
		// 设置寄存器TCFG0的值，低八位先清零，在根据应用程序数据传过来的数据进行低八位设置
		writel((readl(pwm->timer_base + TCFG0) & ~0xff) | (data & 0xff), pwm->timer_base + TCFG0);
		break;
	case SET_CNT:
		writel(data, pwm->timer_base + TCNTB0);		// 设置pwm定时器的初值
		writel(data / 2, pwm->timer_base + TCMPB0); // 设置pwm定时器的翻转值
		writel((readl(pwm->timer_base + TCON) & ~0xf) | 0x2, pwm->timer_base + TCON); // 手动更新, 装载TCNTB0和TCMPB0
		writel(readl(pwm->timer_base + TCON) & ~0x2, pwm->timer_base + TCON);
		break;
	}

	return 0;
}

static struct file_operations fs4412_pwm_fops = {
	.owner = THIS_MODULE,
	.open = fs4412_pwm_open,
	.release = fs4412_pwm_rlease,
	.unlocked_ioctl = fs4412_pwm_ioctl,
};

static int __init fs4412_pwm_init(void)
{
	int ret;
	dev_t devno = MKDEV(PWM_MA, PWM_MI); // 由主次设备号获得dev_t的宏

	ret = register_chrdev_region(devno, PWM_NUM, "pwm"); // 为其分配设备号，为注册设备做准备
	if (ret < 0)
	{
		printk("faipwm : register_chrdev_region\n");
		return ret;
	}

	pwm = kmalloc(sizeof(*pwm), GFP_KERNEL); // 在内核空间为指针申请地址
	if (pwm == NULL)
	{
		ret = -ENOMEM;
		printk("faipwm: kmalloc\n");
		goto err1;
	}
	memset(pwm, 0, sizeof(*pwm)); // 指针初始化

	cdev_init(&pwm->cdev, &fs4412_pwm_fops); // 注册文件操作字符集，建立cdev和file_operation之间的连结
	pwm->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pwm->cdev, devno, PWM_NUM); // 注册设备
	if (ret < 0)
	{
		printk("faipwm: cdev_add\n");
		goto err2;
	}

	pwm->gpdcon = ioremap(GPDCON, 4); // 将控制蜂鸣器引脚功能的控制寄存器地址映射为虚拟地址
	if (pwm->gpdcon == NULL)
	{
		ret = -ENOMEM;
		printk("faipwm: ioremap gpdcon\n");
		goto err3;
	}

	pwm->timer_base = ioremap(TIMER_BASE, 4); // 将定时器的基地址映射为虚拟地址
	if (pwm->timer_base == NULL)
	{
		ret = -ENOMEM;
		printk("failed: ioremap timer_base\n");
		goto err4;
	}

	// fs4412_pwm_init();
	return 0;

err4:
	iounmap(pwm->gpdcon);
err3:
	cdev_del(&pwm->cdev);
err2:
	kfree(pwm);
err1:
	unregister_chrdev_region(devno, PWM_NUM);
	return ret;
}

static void __exit fs4412_pwm_exit(void)
{
	dev_t devno = MKDEV(PWM_MA, PWM_MI);
	iounmap(pwm->timer_base);
	iounmap(pwm->gpdcon);					  // 取消地址映射，对应fs4412_pwm_init()中的ioremap()
	cdev_del(&pwm->cdev);					  ////注销设备  对应fs4412_pwm_init()中的cdev_add（）
	kfree(pwm);								  // 释放pwm指针
	unregister_chrdev_region(devno, PWM_NUM); // 释放fs4412_pwm_init()中通过register_chrdev_region（）注册的设备号
}

module_init(fs4412_pwm_init);
module_exit(fs4412_pwm_exit);
