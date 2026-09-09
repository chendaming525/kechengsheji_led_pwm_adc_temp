# FS4412 + Windows Qt 部署说明

本文按当前仓库源码及提供的截图编写：Windows 运行 `app/ledcontrol`，Linux 虚拟机在 `/CBT-SuperIOT/work` 中用 `arm-linux-gcc` 将 `tcp_client1.c` 编译为 `tcp_client`，再复制到 FS4412 Linux 运行。板子主动连接电脑的 TCP 8888 端口。下文分别注明虚拟机和板端操作；示例 IP、板端存放目录需要按现场调整。本文未在虚拟机或实物板上验证。

本项目中，LED 通过 GPIO 开关实现提醒；PWM 用于驱动蜂鸣器播放音乐和提示音，不需要 LED 亮度调节。

## 1. 准备文件与运行环境

电脑运行目录使用已经编译好的 `app/build/Qt_4_8_7_MinGW_Release/release/`，应包含：

```text
ledcontrol.exe
QtCore4.dll
QtGui4.dll
QtNetwork4.dll
libgcc_s_dw2-1.dll
libstdc++-6.dll
libwinpthread-1.dll
style.qss
config.ini
```

将 `app/config.ini`、`app/style.qss` 复制到 exe 旁边。修改这份 `config.ini` 的对应项，其他内容保留：

```ini
[system]
simulation=0
```

这里 `simulation=0` 使用真实板端数据和音乐控制，`simulation=1` 用于 PC 演示。温度由下位机 TCP 上报，蜂鸣器和 GPIO 由板端 SDK 控制。配置只保留运行模式、温度报警阈值/回差和 ADC 满量程。

从 exe 所在目录启动程序。程序优先读取“当前工作目录”的 config.ini，其次读取 exe 旁的文件；改错副本会导致设置不生效。改配置后重启 Qt。

板端需要 Linux、对应驱动，以及下面的设备：

| 设备 | 当前用途 | 来源 |
| --- | --- | --- |
| `/dev/led` | LED 开关 | 本仓库 LED 驱动 |
| `/dev/pwm` | 蜂鸣器 PWM | 本仓库 PWM 驱动 |
| `/dev/adc` | 电位器采样 | 本仓库 ADC 驱动，代码选择 AIN3 |
| `/dev/ds18b20` | 温度采集 | 需板卡配套驱动，本仓库未找到该驱动源码 |
| `/dev/input/event0` | 按键事件 | 板卡内核的 input 驱动 |

## 2. 配置电脑和板子的 IP

示例：电脑 `192.168.110.200`，板子 `192.168.110.201`，掩码 `255.255.255.0`。两端地址不能重复，且应接入可互通的网络。

电脑执行 `ipconfig`，查找连接开发板那张网卡的 IPv4 地址。如果已经有可互通的地址，直接沿用。网线直连时，可在 Windows 网卡的 IPv4 属性中设置上述电脑地址及掩码；同网段直连通信不需要网关或 DNS。

在板子的串口终端执行（先用 `ifconfig -a` 确认网卡名）：

```sh
ifconfig eth0 192.168.110.201 netmask 255.255.255.0 up
ping -c 4 192.168.110.200
```

这是临时配置，重启可能丢失；长期部署需写入板卡系统实际使用的网络启动配置。通过 SSH 改地址可能中断连接，首次配置用串口更方便。

修改 `sdk/tcp_client1.c` 顶部：

```c
#define SERVER_IP "192.168.110.200"  // 填电脑 IP，不是板子 IP
#define PORT 8888
```

当前程序没有解析命令行 IP 参数，不能用 `./tcp_client 192.168.110.200` 修改连接地址；修改宏后需要重新编译。Qt 在 `app/mainwindow.cpp` 的 `startTcpServer()` 中监听 `QHostAddress::Any, 8888`，通常不需要改监听 IP。换端口则两端一起改、一起重编。编译前确认虚拟机 work 中的源码副本已经同步了 IP 修改。

启动 Qt 后，在电脑 PowerShell 检查监听：

```powershell
Get-NetTCPConnection -LocalPort 8888 -State Listen
```

如 Windows 防火墙阻止连接，可在“高级安全 Windows Defender 防火墙 → 入站规则 → 新建规则”中，为该 exe 放行 TCP 本地端口 8888，远程地址限制为板子的 IP，并应用到实际使用的网络配置文件。不必关闭整个防火墙。ping 失败也可能只是 ICMP 被拦截，应结合 TCP 连接结果判断。

## 3. 按截图在虚拟机编译下位机程序

### 3.1 创建 work 目录并复制文件（虚拟机操作）

打开配套 Linux 虚拟机终端：

```sh
mkdir -p /CBT-SuperIOT/work
cd /CBT-SuperIOT/work
```

通过共享目录、拖拽或 U 盘，将仓库中的文件按下表复制到 work：

| 仓库来源 | 虚拟机目标 |
| --- | --- |
| `sdk/tcp_client1.c` | `/CBT-SuperIOT/work/tcp_client1.c` |
| `sdk/fs4412_zh/driver_led/` 整个目录 | `/CBT-SuperIOT/work/driver_led/` |
| `sdk/fs4412_zh/driver_pwm/` 整个目录 | `/CBT-SuperIOT/work/driver_pwm/` |
| `sdk/fs4412_zh/driver_adc/` 整个目录 | `/CBT-SuperIOT/work/driver_adc/` |
| `sdk/fs4412_zh/pwm_music.h` | `/CBT-SuperIOT/work/pwm_music.h` |

目录结构与截图一致：

```text
/CBT-SuperIOT/work/
├── tcp_client1.c
├── driver_led/
│   ├── fs4412_led.h
│   ├── fs4412_led.c
│   └── Makefile
├── driver_pwm/
│   ├── fs4412_pwm.h
│   ├── fs4412_pwm.c
│   └── Makefile
├── driver_adc/
├── pwm_music.h
└── tcp_client           # 编译成功后生成
```

当前 tcp_client1.c 已内置旋律，没有引用 pwm_music.h；这里保留该文件以对应截图，不需要新增 include。

### 3.2 调整 work 副本的头文件路径

仓库源码的 include 带有 `fs4412_zh/` 前缀，截图中的三个驱动目录却直接位于 work 下。因此，在虚拟机 `/CBT-SuperIOT/work/tcp_client1.c` 中将：

```c
#include "fs4412_zh/driver_led/fs4412_led.h"
#include "fs4412_zh/driver_pwm/fs4412_pwm.h"
```

改成：

```c
#include "driver_led/fs4412_led.h"
#include "driver_pwm/fs4412_pwm.h"
```

同时确认 `SERVER_IP` 是运行 Qt 的 Windows 电脑 IP，端口是 8888。这里调整的是虚拟机中的平铺部署副本；仓库仍保留原目录结构。以后重新复制源码到 work 时，也要同步调整这两行。

### 3.3 执行截图中的编译命令（虚拟机操作）

```sh
cd /CBT-SuperIOT/work
arm-linux-gcc --version
arm-linux-gcc tcp_client1.c -o tcp_client -lpthread
```

`tcp_client1.c` 是源码名，`-o tcp_client` 指定输出程序名；`-lpthread` 链接 pthread_create、pthread_detach 和互斥锁所需的线程库。命令中的参数使用英文半角短横线 `-`。

若旧版编译器提示 for 循环变量声明需要 C99，使用下面的兼容命令：

```sh
arm-linux-gcc -std=gnu99 tcp_client1.c -o tcp_client -lpthread
```

编译没有报错后检查产物：

```sh
ls -l tcp_client
file tcp_client
```

`file` 应显示 ARM 架构的 ELF 可执行文件。若提示 `arm-linux-gcc: command not found`，需要加载配套虚拟机的工具链环境或将其实际 bin 目录加入 PATH。

### 3.4 将产物复制到板子

在板端创建存放目录：

```sh
mkdir -p /root/ledcontrol/work
```

通过 U 盘、现有共享目录或 SFTP 将虚拟机生成的 `tcp_client` 放入此目录。如果板子开启 SSH 且虚拟机能访问板子，可在虚拟机执行：

```sh
scp /CBT-SuperIOT/work/tcp_client root@192.168.110.201:/root/ledcontrol/work/
```

板端随后执行 `chmod +x /root/ledcontrol/work/tcp_client`。程序在 ARM 板子上运行；普通 x86 虚拟机用于交叉编译。驱动加载和正式启动见下面两节。

## 4. 编译、加载驱动

如果板上已经有正常工作的配套驱动和设备节点，先使用现有驱动。应用程序编译与内核模块编译是两回事：`.ko` 必须匹配板上正在运行的内核版本、配置和工具链，不能使用电脑自己的 Linux 内核目录。

板端查看：

```sh
uname -r
lsmod
ls -l /dev/led /dev/pwm /dev/adc /dev/ds18b20
cat /proc/devices
```

仓库三个驱动 Makefile 的默认 `KERNELDIR` 是 `/CBT-SuperIOT/linux-3.14-fs4412`。按截图布局，在虚拟机 work 中编译；确认该目录确实是与板端匹配、已配置构建的内核目录，否则修改 BOARD_KERNEL：

```sh
cd /CBT-SuperIOT/work
BOARD_KERNEL=/CBT-SuperIOT/linux-3.14-fs4412
BOARD_CROSS=arm-linux-
make -C driver_led KERNELDIR="$BOARD_KERNEL" ARCH=arm CROSS_COMPILE="$BOARD_CROSS"
make -C driver_pwm KERNELDIR="$BOARD_KERNEL" ARCH=arm CROSS_COMPILE="$BOARD_CROSS"
make -C driver_adc KERNELDIR="$BOARD_KERNEL" ARCH=arm CROSS_COMPILE="$BOARD_CROSS"
```

若在板上本机编译模块，也需要对应的内核构建目录，并使用 `ARCH=arm CROSS_COMPILE=`。只有 gcc 而没有匹配内核构建文件仍然无法编译驱动。

将三个生成的 `.ko`（各 driver 目录内）复制到板子的 `/root/ledcontrol/work/`，与 tcp_client 放在一起。以 root 身份，在尚未加载这些模块时执行：

```sh
cd /root/ledcontrol/work
insmod fs4412_led.ko
insmod fs4412_pwm.ko
insmod fs4412_adc.ko
dmesg | tail -n 30
```

本仓库这三个驱动没有自动创建设备节点，且使用固定设备号。确认模块加载成功、`/proc/devices` 中注册匹配后，仅对不存在的节点执行：

```sh
test -e /dev/led || mknod /dev/led c 500 0
test -e /dev/pwm || mknod /dev/pwm c 501 0
test -e /dev/adc || mknod /dev/adc c 502 0
ls -l /dev/led /dev/pwm /dev/adc
```

这些号码只适用于当前源码；若使用其他版本驱动，以其实际设备号为准。已有节点也要核对号码。`mknod` 只创建入口，不会安装驱动。

`/dev/ds18b20` 使用板卡配套驱动的实际设备号及 ioctl 接口，不能随意套用一个号码。按键设备用 `cat /proc/bus/input/devices` 确认，若不是 event0，修改 `key_monitor_thread()` 的 open 路径后重编；当前按键码映射是 114/115/116，需要与实际驱动一致。

## 5. 启动与验证

每次开机按这个顺序：配置板端网络 → 加载驱动/检查节点 → 启动电脑 Qt → 启动板端客户端。

在板端执行：

```sh
cd /root/ledcontrol/work
chmod +x tcp_client
./tcp_client
```

预期板端打印“已连接到 Qt 上位机”，Qt 日志显示下位机连接。转动电位器后，板端约每 3 秒发送一条 `adc=...%`；点击音乐按钮，应收到 `play_music`；控制提醒灯时应收到 `LED_ON` 或 `LED_OFF`。

先前台运行，方便看报错。确认正常后，需要后台运行且系统有 nohup 时可以使用：

```sh
nohup ./tcp_client > tcp_client.log 2>&1 &
echo $!
```

记录输出的 PID，停止时用 `kill PID`。前台调试用 Ctrl+C。当前程序没有自动重连，Qt 重启或连接断开后，需要重新启动客户端。当前退出路径没有可靠地保证关灯、关蜂鸣器，结束演示前先完成关灯并等待音乐结束。

## 6. 使用 SDK 默认 LED

直接使用当前 SDK 的默认配置：`/dev/led` 控制编号 1 的灯，对应 GPX2_7（项目标注为板载 LED2），高电平点亮。

按第 4 节加载 LED 驱动并检查设备节点，再按第 5 节启动 Qt 和板端 `tcp_client`。在 Qt 界面操作提醒灯，确认板载 LED 能正常点亮和关闭即可。

## 7. 使用 SDK 默认蜂鸣器播放音乐

直接使用当前 SDK 的默认 PWM 蜂鸣器配置。按第 4 节加载 PWM 驱动并检查 `/dev/pwm`，将 Qt 的 `config.ini` 设置为 `simulation=0`，再按第 5 节启动并连接程序。

点击 Qt 的“试听喂食提示音乐”按钮，确认板端收到 `play_music`，蜂鸣器播放内置《两只老虎》即可。

### 定时播放音乐

在“定时喂食”卡片中输入小时和分钟，点击“添加”。可以添加多个时间，每天到点各触发一次；选中列表中的时间后可以删除，“清空”会移除全部提醒。时间会自动保存到当前 config.ini 的 `[schedule] times`，重启后恢复。

“当前时间”使用电脑本地系统时间并每秒刷新。验证时添加下一分钟的时间，保持 Qt 运行；真实板模式需先连接下位机，到点发送 `play_music` 播放音乐。定时提醒只播放音乐，不修改食盆余量或喂食次数。模拟模式使用电脑提示音；高温报警期间音乐按钮的原有优先级规则仍然适用。程序关闭时不会触发提醒，长时间休眠后不补播已过期提醒。

## 8. 常见问题

| 现象 | 检查方法 |
| --- | --- |
| `open led device` 失败，程序退出 | 检查 LED 模块、/dev/led 设备号和权限；客户端在连接网络前先打开 LED |
| `open pwm device` 失败 | 检查 PWM 驱动和节点；客户端可能继续连接，但不会发声 |
| 连接被拒绝 | 先启动 Qt，确认监听 8888，检查防火墙和目标 IP |
| 连接超时 | 检查网线、网卡地址、网段、路由和防火墙 |
| `Exec format error` | 可执行文件架构不匹配，重新使用板端 gcc 或匹配的 ARM 工具链 |
| 文件存在却提示 `not found` | 除路径外，检查交叉编译产物要求的动态加载器和板上运行库是否匹配 |
| `Invalid module format` | 检查 dmesg，模块必须与运行内核匹配 |
| 音乐只在电脑响 | 确认实际加载的 config.ini 设置 simulation=0，重启 Qt |
| LED 只有开/关 | 符合本项目用途；PWM 专用于蜂鸣器音乐和提示音 |
| ADC 无数据或客户端 CPU 占用高 | 检查 /dev/adc；当前 ADC 打开失败后循环没有休眠，应先修复驱动/节点再运行 |
| 温度没有数据 | 检查配套 ds18b20 驱动及 ioctl 契约；客户端温度路径目前直接写在 open 中，仅改 TEMP_PATH 宏无效 |
| 改 LED_DEVICE 宏没有效果 | 当前 main 直接 open("/dev/led", ...)，需要修改实际 open 调用或让其使用宏 |
| 连续操作偶发丢命令 | 当前板端按一次 recv 处理一条命令，尚未按换行完整处理 TCP 粘包/拆包 |

本文完成了源码层面的配置核对；实际驱动加载、引脚对应关系、温度返回值及硬件功能仍需在你的板卡上联调。
