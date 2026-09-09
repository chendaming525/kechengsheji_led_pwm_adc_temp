# ledcontrol 项目说明

`ledcontrol` 是一个基于 Qt 的 FS4412 上位机/下位机控制示例项目。项目当前按两个目录组织：

```text
app/
  Qt 上位机程序，可以作为独立 Qt 工程打开和运行

sdk/
  FS4412 下位机程序、Linux 字符设备驱动和参考代码
```

整体目标是：Qt 上位机负责界面显示和发送控制命令，FS4412 下位机负责采集硬件数据、控制 LED/PWM/蜂鸣器，并通过 TCP 把结果返回给 Qt。

## 1. 总体架构

```text
Qt 上位机 app/
  负责：界面显示、按钮操作、数据显示、日志输出、发送控制命令、接收下位机数据

TCP 文本协议
  负责：在 Qt 上位机和 FS4412 下位机之间传递字符串命令和状态数据

FS4412 下位机 sdk/tcp_client1.c
  负责：连接 Qt 上位机、接收控制命令、采集 ADC/温度/按键、控制 LED/PWM/蜂鸣器

Linux 字符设备驱动 sdk/fs4412_zh/
  负责：通过 /dev/led、/dev/pwm、/dev/adc、/dev/ds18b20 操作真实硬件
```

通信方向：

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

## 2. 目录说明

```text
app/ledcontrol.pro
  Qt 上位机工程文件

app/main.cpp
  Qt 程序入口

app/mainwindow.h / app/mainwindow.cpp
  Qt 界面、TCP Server、按钮逻辑、数据显示逻辑

app/hardware.h / app/hardware.cpp
  上位机配置读取：运行模式、温度报警阈值/回差和 ADC 满量程；硬件由板端 SDK 控制

app/config.ini
  Qt 上位机配置文件

app/style.qss
  Qt 界面样式

sdk/tcp_client1.c
  FS4412 下位机 TCP Client，负责接收上位机命令和返回硬件结果

sdk/fs4412_zh/
  LED、PWM、ADC 驱动和测试参考代码
```

## 3. Qt 上位机说明

`app` 目录可以作为独立 Qt 工程使用，使用 Qt Creator 打开：

```text
app/ledcontrol.pro
```

Qt 上位机启动后会监听 TCP 端口：

```text
8888
```

相关代码在：

```text
app/mainwindow.cpp -> startTcpServer()
app/mainwindow.cpp -> onTcpNewConnection()
app/mainwindow.cpp -> onTcpReadyRead()
app/mainwindow.cpp -> handleLowerMessage()
app/mainwindow.cpp -> sendCommand()
```

Qt 上位机主要做三件事：

```text
1. 显示宠物喂食提醒界面
2. 给下位机发送控制命令
3. 接收下位机返回的 ADC、温度、按键数据并刷新界面
```

## 4. FS4412 下位机说明

下位机核心文件：

```text
sdk/tcp_client1.c
```

它会主动连接 Qt 上位机：

```c
#define SERVER_IP "192.168.110.200"
#define PORT 8888
```

现场使用时，需要把 `SERVER_IP` 改成 Qt 上位机所在电脑的 IP。

下位机连接成功后，通过全局 socket：

```c
int sock;
```

和 Qt 上位机通信。

## 5. TCP 通信协议

本项目使用简单文本协议：

```text
一条消息 = 一行文本
```

每条消息末尾使用：

```text
\n
```

作为结束符。

### 5.1 Qt 上位机发送给下位机

```text
LED_ON
LED_OFF
BUZZER_FREQ=880
BUZZER_OFF
play_music
```

含义：

```text
LED_ON            打开 LED
LED_OFF           关闭 LED
BUZZER_FREQ=880   设置蜂鸣器频率
BUZZER_OFF        关闭蜂鸣器
play_music        播放内置《两只老虎》旋律
```

### 5.2 下位机返回给 Qt 上位机

```text
LED_OFF
adc=62.50%
temperature=26.75
key=k1
key=k2
key=k3
```

含义：

```text
adc=xx%          ADC 百分比，用来显示食盆余量
temperature=xx   温度值
key=k1/k2/k3     下位机按键事件
```

## 6. 下位机如何返回硬件结果

下位机返回硬件结果的核心函数是：

```c
static void send_line(const char *msg)
{
    pthread_mutex_lock(&sock_mutex);
    write(sock, msg, strlen(msg));
    write(sock, "\n", 1);
    pthread_mutex_unlock(&sock_mutex);
}
```

它的作用是：

```text
把一条文本消息发给 Qt 上位机，并在末尾加换行符
```

三个采集线程最终都会调用 `send_line()`。

### 6.1 按键结果返回

线程：

```c
void* key_monitor_thread(void* arg)
```

流程：

```text
/dev/input/event0
  -> read()
  -> 判断按键编号
  -> 组装 "key=k1"
  -> send_line()
  -> Qt 上位机
```

关键代码：

```c
case 114: snprintf(msg, sizeof(msg), "key=k1"); break;
case 115: snprintf(msg, sizeof(msg), "key=k2"); break;
case 116: snprintf(msg, sizeof(msg), "key=k3"); break;
send_line(msg);
```

### 6.2 ADC 结果返回

线程：

```c
void* adc_send_thread(void* arg)
```

流程：

```text
/dev/adc
  -> read()
  -> 得到 ADC 原始值
  -> 换算成百分比
  -> 组装 "adc=62.50%"
  -> send_line()
  -> Qt 上位机
```

关键代码：

```c
read(fd, &data, sizeof(data));
percentage = (float)data / 4096.0 * 100.0;
snprintf(buffer, sizeof(buffer), "adc=%.2f%%", percentage);
send_line(buffer);
```

### 6.3 温度结果返回

线程：

```c
void* temp_send_thread(void* arg)
```

流程：

```text
/dev/ds18b20
  -> ioctl()
  -> 得到温度原始值
  -> 换算成摄氏度
  -> 组装 "temperature=26.75"
  -> send_line()
  -> Qt 上位机
```

关键代码：

```c
ioctl(fd, GPIO_ON, temp);
tempvalue = (float)temp[1] * 0.0625;
snprintf(buffer, sizeof(buffer), "temperature=%.2f", tempvalue);
send_line(buffer);
```

## 7. ioctl 简单理解

Linux 下很多硬件设备会被抽象成文件：

```text
/dev/led
/dev/pwm
/dev/adc
/dev/ds18b20
/dev/input/event0
```

应用程序常用：

```c
open()
read()
write()
ioctl()
close()
```

其中 `ioctl()` 可以理解为：

```text
应用程序给设备驱动发送特殊控制命令
```

例如：

```c
int i = 1;
ioctl(fd, LED_ON, &i);
```

意思是：

```text
告诉 /dev/led 对应的驱动：打开第 1 个 LED
```

`LED_ON` 的定义类似：

```c
#define LED_ON _IOW(LED_MAGIC, 0, int)
```

可以理解为生成一个 ioctl 命令编号：

```text
LED_MAGIC   设备类别暗号
0           该设备里的第 0 号命令
int         这个命令携带一个 int 参数
_IOW        方向是应用程序写给驱动
```

一句话记：

```text
ioctl = 应用程序和设备驱动之间的“控制命令通道”
```

## 8. PWM 音乐播放

Qt 上位机点击“试听喂食提示音乐”时，真实下位机模式下会发送：

```text
play_music
```

下位机收到后执行：

```c
play_music();
```

音乐旋律直接写在：

```text
sdk/tcp_client1.c
```

核心数据结构：

```c
typedef struct {
    int freq;
    int duration_ms;
} MusicNote;
```

内置旋律：

```c
static const MusicNote twotigers_music[] = {
    {293, 350}, {330, 350}, {370, 350}, {293, 350},
    ...
};
```

播放线程：

```c
void* play_music_thread(void* arg)
```

播放每个音符时：

```c
buzzer_set_freq(twotigers_music[i].freq);
usleep(twotigers_music[i].duration_ms * 1000);
buzzer_set_freq(0);
usleep(30000);
```

同时使用 `music_playing` 和 `music_mutex` 防止重复播放。

## 9. Qt 4.8 / C++11 兼容说明

现场机器使用 Qt 4.8，因此 `app` 中做了兼容调整：

```text
CONFIG += c++11
unix: QMAKE_CXXFLAGS += -std=c++11
```

注意：

```text
Qt 4.8 可以配合支持 C++11 的编译器使用，
但 Qt 4.8 自己不支持很多 Qt5/Qt6 API。
```

因此已经避免使用：

```text
Qt5 新式 connect
QOverload
QSignalBlocker
QString::toHtmlEscaped()
nullptr
override
QVector 初始化列表赋值
Qt5 函数指针形式 QTimer::singleShot
```

对应改法：

```text
connect 改为 SIGNAL/SLOT 宏
QSignalBlocker 改为自定义 SignalBlocker
nullptr 改为 0
默认成员初始化改为构造函数初始化
QVector 初始化列表改为 clear() + << 写法
QTimer::singleShot 改为 SLOT(...) 写法
```

## 10. 编码说明

项目中的中文源码和说明文件已统一检查为 UTF-8。

之前部分参考代码是 GB2312/GBK 风格保存，已经转换为 UTF-8，避免中文注释和日志乱码。

## 11. 编译与运行建议

### 11.1 Qt 上位机

在 Qt Creator 中打开：

```text
app/ledcontrol.pro
```

如果只做界面演示，保持：

```ini
[system]
simulation=1
```

如果要和 FS4412 下位机联调，需要保证：

```text
1. Qt 上位机所在电脑和 FS4412 在同一网络
2. 下位机 tcp_client1.c 中 SERVER_IP 改成电脑 IP
3. Qt 上位机先运行并监听 8888
4. FS4412 再运行 tcp_client1
```

### 11.2 FS4412 下位机

下位机程序：

```text
sdk/tcp_client1.c
```

需要在 FS4412 对应交叉编译环境中编译，并确保设备节点存在：

```text
/dev/led
/dev/pwm
/dev/adc
/dev/ds18b20
/dev/input/event0
```

## 12. 当前状态

```text
app/ 可以作为 Qt 上位机工程独立打开
sdk/ 保存下位机进程和驱动参考代码
TCP 协议已统一为一行一条文本消息
下位机可返回按键、ADC、温度数据
Qt 可发送 LED、PWM、蜂鸣器、播放音乐命令
play_music 会触发下位机播放内置《两只老虎》
项目中未发现 TODO 或“由你自己完成”残留
```

## 13. Qt 版本更新参考

当前 `app/` 已按 Qt 4.8 + C++11 做过兼容处理。现场如果 Qt 4.8 编译仍然遇到环境问题，或者希望改回更现代的 Qt5 写法，可以考虑升级 Qt 版本。

Qt 官方历史版本下载地址：

```text
https://download.qt.io/archive/qt/
```

建议现场先尝试使用现有 Qt 4.8 编译；如果需要升级，再优先选择 Qt 5.x 的稳定版本。
