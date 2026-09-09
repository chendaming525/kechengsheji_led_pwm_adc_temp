obj-m:= fs4412_led.o
KERNELDIR :=/CBT-SuperIOT/linux-3.14-fs4412
default:
	make -C $(KERNELDIR) M=$(shell pwd) modules
clean:
	rm -f *.o *.ko *.mod.* modules.* Mo*.* 

