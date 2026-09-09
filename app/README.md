# ledcontrol 修改说明

本文档记录本轮对项目的主要修改，重点说明 Qt 上位机、FS4412 下位机、TCP 通信协议、下位机音乐播放逻辑，以及源码编码处理。

## 1. 总体架构调整

项目现在按“上位机 + 下位机”的方式理解和组织：

```text
Qt 上位机程序
  负责：界面显示、按钮操作、数据显示、日志输出、发送控制命令、接收下位机数据

TCP 文本协议
  负责：在 Qt 上位机和 FS4412 下位机之间传递字符串命令和状态数据

FS4412 下位机进程 refdoc/tcp_client1.c
  负责：连接 Qt 上位机、接收控制命令、采集 ADC/温度/按键、控制 LED/PWM/蜂鸣器

Linux 字符设备驱动
  负责：通过 /dev/led、/dev/pwm、/dev/adc、/dev/ds18b20 操作真实硬件
```

通信方向如下：

```text
用户点击 Qt 按钮
  -> Qt 发送 TCP 命令
  -> tcp_client1.c 接收命令
  -> ioctl/read/write 操作开发板设备
  -> 硬件执行动作

FS4412 采集 ADC/温度/按键
  -> tcp_client1.c 组装字符串
  -> TCP 发送给 Qt
  -> Qt 解析字符串并刷新界面
```

## 2. Qt 上位机修改

### 2.1 增加 Qt Network 模块

文件：`ledcontrol.pro`

修改：

```pro
QT += core gui network
```

目的：让 Qt 工程可以使用 `QTcpServer`、`QTcpSocket` 等 TCP 网络类。

### 2.2 增加 TCP Server

文件：`mainwindow.h`、`mainwindow.cpp`

新增内容：

- `QTcpServer *m_tcpServer`
- `QTcpSocket *m_client`
- `QByteArray m_rxBuffer`
- `startTcpServer()`
- `onTcpNewConnection()`
- `onTcpReadyRead()`
- `onTcpDisconnected()`
- `sendCommand()`
- `handleLowerMessage()`

Qt 上位机启动后监听端口：

```text
8888
```

FS4412 下位机进程 `tcp_client1.c` 会主动连接这个端口。

### 2.3 Qt 发送给下位机的命令

Qt 当前会发送这些文本命令：

```text
LED_ON
LED_OFF
BUZZER_FREQ=880
BUZZER_OFF
play_music
```

说明：

- `LED_ON`：打开 LED
- `LED_OFF`：关闭 LED
- `BUZZER_FREQ=880`：设置蜂鸣器频率
- `BUZZER_OFF`：关闭蜂鸣器
- `play_music`：通知下位机播放内置音乐《两只老虎》

### 2.4 Qt 接收下位机数据

Qt 会解析下位机发来的这些消息：

```text
adc=62.50%
temperature=26.75
key=k1
key=k2
key=k3
```

说明：

- `adc=xx%`：食盆余量或 ADC 百分比，用来刷新进度条和余量显示
- `temperature=xx`：温度数据，保留给温度报警逻辑使用
- `key=k1/k2/k3`：下位机按键事件，可触发 Qt 侧对应动作

### 2.5 音乐按钮行为

真实下位机模式下，Qt 点击“试听喂食提示音乐”时，不再由 Qt 自己循环发送音符，而是只发送一条命令：

```text
play_music
```

实际旋律播放由下位机 `tcp_client1.c` 完成。

## 3. 下位机 tcp_client1.c 修改

文件：`refdoc/tcp_client1.c`

### 3.1 文件编码转换

原文件是 GB2312/GBK 风格保存，已转换为 UTF-8，避免中文注释和日志乱码。

### 3.2 增加 PWM 驱动头文件

原来主要使用 LED 驱动头文件，现在增加了 PWM 驱动相关头文件：

```c
#include "fs4412_zh/driver_led/fs4412_led.h"
#include "fs4412_zh/driver_pwm/fs4412_pwm.h"
```

目的：下位机进程可以同时使用 LED 和 PWM 的 ioctl 命令。

### 3.3 增加 PWM 设备支持

新增：

```c
#define PWM_DEVICE "/dev/pwm"
#define PCLK 0x4200000

int pwm_fd = -1;
pthread_mutex_t pwm_mutex = PTHREAD_MUTEX_INITIALIZER;
```

`main()` 中打开 `/dev/pwm`，并初始化 PWM：

```c
pwm_fd = open(PWM_DEVICE, O_RDWR | O_NONBLOCK);
ioctl(pwm_fd, PWM_OFF);
ioctl(pwm_fd, SET_PRE, &pre);
```

### 3.4 增加统一发送函数

新增：

```c
static void send_line(const char *msg)
```

所有发给 Qt 上位机的数据统一变成：

```text
一条消息 + '\n'
```

这样 Qt 可以按行解析 TCP 数据。

### 3.5 增加命令清理函数

新增：

```c
static void trim_line(char *s)
```

作用：去掉 Qt 发来的命令末尾的 `\n`、`\r`。

否则 Qt 发送：

```text
LED_ON\n
```

下位机用：

```c
strcmp(buffer, "LED_ON")
```

会匹配失败。

### 3.6 修复 recv 缓冲区风险

原来：

```c
recv(sock, buffer, sizeof(buffer), 0);
buffer[len] = '\0';
```

如果刚好收到 1024 字节，`buffer[len]` 会越界。

现在改为：

```c
recv(sock, buffer, sizeof(buffer) - 1, 0);
buffer[len] = '\0';
```

给字符串结束符 `\0` 留出位置。

### 3.7 统一下位机上报协议

按键线程上报：

```text
key=k1
key=k2
key=k3
```

ADC 线程上报：

```text
adc=62.50%
```

温度线程上报：

```text
temperature=26.75
```

这些消息都通过 `send_line()` 发送。

### 3.8 增加蜂鸣器频率控制

新增：

```c
static void buzzer_set_freq(int freq)
```

逻辑：

- `freq <= 0`：关闭 PWM
- `freq > 0`：根据频率计算计数值，调用 `SET_CNT`，再打开 PWM

核心计算：

```c
int cnt = (PCLK / 256 / 4) / freq;
```

### 3.9 增加内置《两只老虎》旋律

新增结构体：

```c
typedef struct {
    int freq;
    int duration_ms;
} MusicNote;
```

新增内置旋律数组：

```c
static const MusicNote twotigers_music[] = {
    {293, 350}, {330, 350}, {370, 350}, {293, 350},
    ...
};
```

每个音符包含：

- `freq`：频率，单位 Hz
- `duration_ms`：持续时间，单位毫秒

### 3.10 增加音乐播放线程

新增：

```c
void* play_music_thread(void* arg)
```

作用：遍历 `twotigers_music` 数组，逐个播放音符。

播放每个音符时：

```c
buzzer_set_freq(twotigers_music[i].freq);
usleep(twotigers_music[i].duration_ms * 1000);
buzzer_set_freq(0);
usleep(30000);
```

### 3.11 增加 play_music 命令

下位机主循环新增识别：

```c
} else if (strcmp(buffer, "play_music") == 0) {
    play_music();
}
```

当 Qt 上位机发送：

```text
play_music
```

下位机会启动线程播放内置《两只老虎》。

### 3.12 防止重复播放

新增：

```c
int music_playing = 0;
pthread_mutex_t music_mutex = PTHREAD_MUTEX_INITIALIZER;
```

配合：

```c
music_is_playing()
music_set_playing()
```

如果音乐正在播放，再收到 `play_music`，会忽略重复触发。

## 4. 音乐配置方案变更

曾经临时新增过外部文件：

```text
refdoc/twotigers_music.ini
```

后续按需求已删除。

现在《两只老虎》的旋律直接写在：

```text
refdoc/tcp_client1.c
```

也就是说，下位机运行时不再依赖额外音乐配置文件。

## 5. 当前 TCP 协议汇总

### 5.1 Qt 上位机 -> FS4412 下位机

```text
LED_ON
LED_OFF
BUZZER_FREQ=880
BUZZER_OFF
play_music
```

### 5.2 FS4412 下位机 -> Qt 上位机

```text
LED_OFF
adc=62.50%
temperature=26.75
key=k1
key=k2
key=k3
```

协议特点：

- 文本协议
- 一条消息一行
- 使用 `\n` 作为消息结束符
- 简单易调试，可以直接用串口日志、网络抓包或 `printf` 查看

## 6. TODO 状态

原来 `refdoc/tcp_client1.c` 中有：

```c
// ==================   pwm声音播放线程 ==================
//由你自己完成
```

现在已经替换为：

```c
// ================== PWM 声音控制 ==================
// Qt 上位机发送 play_music, 这里播放内置的《两只老虎》旋律
```

当前项目中未发现 `TODO` 或 `由你自己完成` 残留。

## 7. 编译验证说明

本次修改已做源码级检查和 UTF-8 编码检查。

当前环境 PATH 中没有可用的：

```text
qmake
mingw32-make
gcc/g++
```

因此没有在本机完成实际编译。后续建议在 Qt Creator 或 FS4412 交叉编译环境中分别验证：

```text
Qt 上位机：使用 Qt Creator 打开 ledcontrol.pro 编译运行
下位机：使用交叉编译器编译 refdoc/tcp_client1.c，并放到 FS4412 上运行
```
