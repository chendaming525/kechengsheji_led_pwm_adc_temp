#ifndef FS4412_PWM_HH
#define FS4412_PWM_HH

#define PWM_MAGIC 'K'
//need arg = 0/1/2/3
#define PWM_ON 	_IO(PWM_MAGIC, 0)
#define PWM_OFF _IO(PWM_MAGIC, 1)
#define SET_PRE _IOW(PWM_MAGIC, 2, int)
#define SET_CNT _IOW(PWM_MAGIC, 3, int)


#define GPDCON 		0x114000A0
#define TIMER_BASE 	0x139D0000
#define TCFG0	0x00
#define TCFG1	0x04
#define TCON	0x08
#define TCNTB0	0x0C
#define TCMPB0	0x10



#endif
