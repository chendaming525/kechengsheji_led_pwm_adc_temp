#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/input.h>

#include "fs4412_zh/driver_led/fs4412_led.h"
#include "fs4412_zh/driver_pwm/fs4412_pwm.h"

#define SERVER_IP "192.168.110.200"         //  替换为 Qt 上位机 IP
#define PORT 8888
#define ADC_DEVICE "/dev/adc"
#define TEMP_PATH "/dev/ds18b20"          // 假设使用 ds18b20 字符设备
#define LED_DEVICE "/dev/led"             // LED 设备节点
#define PWM_DEVICE "/dev/pwm"             // PWM 设备节点
#define PCLK 0x4200000
#define GPIO_ON   _IOW('G', 0, int)
#define GPIO_OFF  _IOW('G', 1, int)

int sock;
int pwm_fd = -1;
int music_playing = 0;
pthread_mutex_t sock_mutex = PTHREAD_MUTEX_INITIALIZER;  // 全局互斥锁，此处初始化
pthread_mutex_t pwm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t music_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int freq;
    int duration_ms;
} MusicNote;

static const MusicNote twotigers_music[] = {
    {293, 350}, {330, 350}, {370, 350}, {293, 350},
    {293, 350}, {330, 350}, {370, 350}, {293, 350},

    {370, 350}, {349, 350}, {440, 700},
    {370, 350}, {349, 350}, {440, 700},

    {440, 175}, {494, 175}, {440, 175}, {349, 175}, {370, 350}, {293, 350},
    {440, 175}, {494, 175}, {440, 175}, {349, 175}, {370, 350}, {293, 350},

    {370, 350}, {220, 350}, {293, 700},
    {370, 350}, {220, 350}, {293, 700},
};

static void trim_line(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == '\0')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void send_line(const char *msg)
{
    pthread_mutex_lock(&sock_mutex);
    write(sock, msg, strlen(msg));
    write(sock, "\n", 1);
    pthread_mutex_unlock(&sock_mutex);
}

static void buzzer_set_freq(int freq)
{
    if (pwm_fd < 0)
        return;

    pthread_mutex_lock(&pwm_mutex);
    if (freq <= 0) {
        ioctl(pwm_fd, PWM_OFF);
    } else {
        int cnt = (PCLK / 256 / 4) / freq;
        ioctl(pwm_fd, SET_CNT, &cnt);
        ioctl(pwm_fd, PWM_ON);
    }
    pthread_mutex_unlock(&pwm_mutex);
}

static int music_is_playing(void)
{
    int playing;
    pthread_mutex_lock(&music_mutex);
    playing = music_playing;
    pthread_mutex_unlock(&music_mutex);
    return playing;
}

static void music_set_playing(int playing)
{
    pthread_mutex_lock(&music_mutex);
    music_playing = playing;
    pthread_mutex_unlock(&music_mutex);
}

// ================== 按键监控线程 ==================

void* key_monitor_thread(void* arg) {
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open key device");
        return NULL;
    }
    struct input_event ev;
    printf("Key monitor thread started\n");  // 添加启动日志

    while (1) {
        // 读取按键事件
        ssize_t bytes_read = read(fd, &ev, sizeof(ev));
        
      if (bytes_read == sizeof(ev) && ev.type == EV_KEY && ev.value == 1) {
        char msg[64];
        switch (ev.code) {
            case 114: snprintf(msg, sizeof(msg), "key=k1"); break;
            case 115: snprintf(msg, sizeof(msg), "key=k2"); break;
            case 116: snprintf(msg, sizeof(msg), "key=k3"); break;
            default: snprintf(msg, sizeof(msg), "key=unknown code=%d", ev.code);
        }
        printf("%s\n", msg);
        send_line(msg);
    }
    }

    close(fd);
    printf("Key monitor thread exited\n");
    return NULL;

}

// ================== ADC 采集线程 ==================
void* adc_send_thread(void* arg) {
    char buffer[1024];
    int data;
    float percentage;
    int fd = open(ADC_DEVICE, O_RDONLY);
    while (1) {
        if (fd >= 0) {
            read(fd, &data, sizeof(data));
            percentage = (float)data / 4096.0 * 100.0;
            snprintf(buffer, sizeof(buffer), "adc=%.2f%%", percentage);
            printf("%s\n", buffer);
            send_line(buffer);
           sleep(3);  // 每3秒发送一次 ADC 值
        }
        
    }
    close(fd);
    return NULL;
}

// ================== 温度采集线程 ==================
void* temp_send_thread(void* arg) {
    int fd = 0;
    unsigned int temp[2] = {0};
    float tempvalue = 0;
    char buffer[256] = {0};
    
    // 打开DS18B20设备
    fd = open("/dev/ds18b20", O_RDWR);
    if (fd == -1) {
        perror("open ds18b20 error");
        return NULL;  // 显式返回NULL
    }
    printf("Temperature thread started\n");
    while (1) {
        // 执行IOCTL操作（假设GPIO_ON是正确的命令）
        if (ioctl(fd, GPIO_ON, temp) < 0) {
            perror("ioctl failed");
            sleep(1);
            continue;
        }
        temp[1] = ioctl(fd, GPIO_ON, temp);
        // 处理温度数据
        if (temp[1] & 0x8000) {
            // 负数温度
            temp[1] = ~temp[1] + 1;
            temp[1] &= 0xffff;
            tempvalue = (float)temp[1] * 0.0625;
            snprintf(buffer, sizeof(buffer), "temperature=-%.2f", tempvalue);
        } else {
            // 正数温度
            temp[1] &= 0xffff;
            tempvalue = (float)temp[1] * 0.0625;
            snprintf(buffer, sizeof(buffer), "temperature=%.2f", tempvalue);
        }
        
        printf("%s\n", buffer);
        // 发送温度数据
        send_line(buffer);
        sleep(3);  // 每隔3秒采集一次
    }
    
    close(fd);
    printf("Temperature thread exited\n");
    return NULL;  // 显式返回
}

// ================== PWM 声音控制 ==================
// Qt 上位机发送 play_music, 这里播放内置的《两只老虎》旋律

void* play_music_thread(void* arg)
{
    size_t i;

    (void)arg;

    printf("开始播放《两只老虎》\n");
    for (i = 0; i < sizeof(twotigers_music) / sizeof(twotigers_music[0]); ++i) {
        buzzer_set_freq(twotigers_music[i].freq);
        usleep(twotigers_music[i].duration_ms * 1000);
        buzzer_set_freq(0);
        usleep(30000);
    }

    buzzer_set_freq(0);
    music_set_playing(0);
    printf("《两只老虎》播放结束\n");
    return NULL;
}

static void play_music(void)
{
    pthread_t tid;

    if (music_is_playing()) {
        printf("《两只老虎》正在播放, 忽略重复触发\n");
        return;
    }

    music_set_playing(1);
    if (pthread_create(&tid, NULL, play_music_thread, NULL) != 0) {
        perror("pthread_create play_music_thread failed");
        music_set_playing(0);
        return;
    }
    pthread_detach(tid);
}

int main(int argc, char **argv)
{
    // 打开 LED 设备文件
    int fd = open("/dev/led", O_RDWR);
    if (fd < 0) {
        perror("open led device");
        exit(EXIT_FAILURE);
    }

    pwm_fd = open(PWM_DEVICE, O_RDWR | O_NONBLOCK);
    if (pwm_fd < 0) {
        perror("open pwm device");
    } else {
        int pre = 255;
        ioctl(pwm_fd, PWM_OFF);
        ioctl(pwm_fd, SET_PRE, &pre);
    }

  if (pthread_mutex_init(&sock_mutex, NULL) != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&pwm_mutex, NULL) != 0) {
        perror("pthread_mutex_init pwm_mutex failed");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&music_mutex, NULL) != 0) {
        perror("pthread_mutex_init music_mutex failed");
        exit(EXIT_FAILURE);
    }
    // 创建 TCP socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            printf("Invalid address / Address not supported\n");
            close(sock);
            exit(EXIT_FAILURE);
        }
    // 连接服务器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("连接失败");
        close(sock);
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("已连接到 Qt 上位机\n");

     // 创建线程

  pthread_t key_thread, adc_thread, temp_thread;
// 创建按键监控线程,并检查错误
if (pthread_create(&key_thread, NULL, key_monitor_thread, NULL) != 0) {
    perror("pthread_create key_thread failed");
    exit(EXIT_FAILURE);
}

// 创建ADC采集线程，并检查错误
if (pthread_create(&adc_thread, NULL, adc_send_thread, NULL) != 0) {
    perror("pthread_create adc_thread failed");
    exit(EXIT_FAILURE);
}

// 创建温度采集线程，并检查错误
if (pthread_create(&temp_thread, NULL, temp_send_thread, NULL) != 0) {
    perror("pthread_create temp_thread failed");
    exit(EXIT_FAILURE);
}

    // 初始上报状态
    const char *init_state = "LED_OFF";
    send_line(init_state);

      // ========== 主循环处理 LED 控制 ==========
    char buffer[1024];
    while (1) {
        int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            printf("服务器断开或接收失败\n");
            exit(EXIT_FAILURE);
        }
        buffer[len] = '\0';
        trim_line(buffer);
        printf("收到命令: %s\n", buffer);

        if (strcmp(buffer, "LED_ON") == 0) {
            int i = 1;
            ioctl(fd,LED_ON, &i);  // 开灯
            printf("设置 LED 为 ON\n");
        } else if (strcmp(buffer, "LED_OFF") == 0) {
            int i = 1;
            ioctl(fd,LED_OFF, &i); // 关灯
            printf("设置 LED 为 OFF\n");
        } else if (strncmp(buffer, "PWM=", 4) == 0) {
            int percent = atoi(buffer + 4);
            printf("收到 PWM 亮度命令: %d%%\n", percent);
            // 当前参考 LED 驱动是 GPIO 灯, 这里先把 0 视为关灯, 大于 0 视为开灯。
            int i = 1;
            ioctl(fd, percent > 0 ? LED_ON : LED_OFF, &i);
        } else if (strncmp(buffer, "BUZZER_FREQ=", 12) == 0) {
            int freq = atoi(buffer + 12);
            buzzer_set_freq(freq);
            printf("设置蜂鸣器频率: %d Hz\n", freq);
        } else if (strcmp(buffer, "BUZZER_OFF") == 0) {
            buzzer_set_freq(0);
            printf("关闭蜂鸣器\n");
        } else if (strcmp(buffer, "play_music") == 0) {
            play_music();
        }
   }

    // 清理资源
    // 清理互斥锁
    if (pwm_fd >= 0) {
        ioctl(pwm_fd, PWM_OFF);
        close(pwm_fd);
    }
    pthread_mutex_destroy(&music_mutex);
    pthread_mutex_destroy(&pwm_mutex);
    pthread_mutex_destroy(&sock_mutex);
    close(sock);
    close(fd);
    return 0;
}
