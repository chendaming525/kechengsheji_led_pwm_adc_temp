#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include "fs4412_adc.h"

MODULE_LICENSE("GPL");


void __iomem *adc_base;
unsigned int adc_major = 502;
unsigned int adc_minor = 0;
struct cdev cdev;

static int fs4412_adc_open(struct inode *inode, struct file *file)
{
	return 0;
}
	
static int fs4412_adc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int fs4412_adc_io_init(void)
{
	writel(3, adc_base + ADCMUX); //设置A/D转换通道为AIN3，即：设置ADCMUX[3:0] = 0x11
	return 0;
}

static ssize_t fs4412_adc_read(struct file *file, char *buf, size_t count, loff_t *loff)
{
	int data = 0;

	if (count != 4)
		return -EINVAL;

	//设置AD转换精度，如果精确度为12位，则设置	ADCCON[16] =0x1。
	//设置预分频值为255，则设置ADCCON[13：6]=0xFF。
	//设置使用预分频,则设置ADCCON[14] =0x1。
	//设置AD转换手动启动,设置ADCCON[0] =0x0
	writel(1<<0 | 1<<14 | 0xff<<6 | 0x1<<16, adc_base + ADCCON);
	while (!(readl(adc_base + ADCCON) >> 15 & 0x1)) //查询AD转化是否完成，如果没有完成继续等待，直到完成为止
	{
		msleep(1);//等待1毫秒
	}
	    data = readl(adc_base + ADCDAT) & 0xfff;  //当AD转化完成时，以上while循环退出 ，读取数据寄存器的值，该值为AD转化后的数字量
	printk("data = %x\n", data);
	if (copy_to_user(buf, &data, sizeof(data))) //把数据值拷贝给应用程序传过来的buf指针
		return -EFAULT;
	return count;
}


struct file_operations fs4412_adc_fops = {
	.owner = THIS_MODULE,
	.open = fs4412_adc_open,
	.release = fs4412_adc_release,
	.read = fs4412_adc_read,

};

static int __init fs4412_adc_init(void)
{
	int ret; 
	dev_t devno = MKDEV(adc_major, adc_minor);
	ret = register_chrdev_region(devno, 1, "adc");//为其分配设备号，为注册设备做准备
	if (ret < 0) {
		printk("faiadc : register_chrdev_region\n");
		return ret;
	}
	
	adc_base = ioremap(ADCBASE, 0x20);  ////将ADC相关寄存器的基地址映射为虚拟地址
	if (adc_base == NULL) {
		printk("failed to ioremap address reg\n");
		goto err1;
	};

	cdev_init(&cdev, &fs4412_adc_fops);//注册文件操作字符集，建立cdev和file_operation之间的连结
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, devno, 1);//注册设备
	if (ret < 0) {
		printk("failed add device\n");
		goto err2;
	}
	fs4412_adc_io_init();
	return 0;
err2:
	iounmap(adc_base);
err1:
	unregister_chrdev_region(devno, 1);
	return ret;
}


static void __exit fs4412_adc_exit(void)
{
	dev_t devno = MKDEV(adc_major, adc_minor);
	iounmap(adc_base);
	cdev_del(&cdev);////注销设备  对应fs4412_adc_init()中的cdev_add（）
	unregister_chrdev_region(devno, 1);//释放fs4412_adc_init()中通过register_chrdev_region（）注册的设备号

}

module_init(fs4412_adc_init);
module_exit(fs4412_adc_exit);



