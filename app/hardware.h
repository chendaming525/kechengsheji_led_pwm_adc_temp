#ifndef HARDWARE_H
#define HARDWARE_H

#include <QString>
#include <QVector>

/* ============================================================
 * 硬件配置结构体: 从同目录 config.ini 读取
 * LED 引脚 / 蜂鸣器引脚(PWM通道) / DS18B20 / ADC 等全部可配置
 * ============================================================ */
struct HardwareConfig
{
    HardwareConfig();

    // ---- [system] ----
    bool simulation;        // 模拟模式(无开发板也能演示)

    // ---- [led] 灯 (FS4412 默认 LED2 = GPX2_7) ----
    int  ledGpio;      // 灯的 GPIO 编号(开关灯)
    bool ledActiveHigh;     // 高电平点亮?
    int  ledPwmChannel;       // 灯的 PWM 通道(调光), -1 表示纯 GPIO 灯

    // ---- [buzzer] 蜂鸣器 ----
    int  buzzerPwmChannel;     // 蜂鸣器的 PWM 通道(引脚)
    int  buzzerFreq;   // 发声频率 Hz
    int  buzzerDutyPercent;    // 占空比 %

    // ---- [pwm] ----
    int  pwmChip;     // /sys/class/pwm/pwmchip<pwmChip>

    // ---- [temperature] DS18B20 ----
    QString tempSource;  // sim | node | bitbang
    QString tempNode;              // node 方式节点路径(w1_slave 等)
    int     tempGpio;   // bitbang: DQ = GPX0_6
    double  tempThreshold;   // 报警阈值 ℃
    double  tempHysteresis;   // 回差 ℃

    // ---- [alarm] 报警 LED ----
    QVector<int> alarmLeds;        // 报警时闪烁的 LED GPIO 列表

    // ---- [adc] ----
    QString adcPath;               // ADC 原始值节点, 空表示未配置
    int  adcMax;  // ADC 满量程

    /* 从文件读取, 缺省键使用上面的默认值; 返回文件是否存在 */
    bool load();
    bool loadFrom(const QString &file);

    QString pwmDir()    const;     // /sys/class/pwm/pwmchip0
    QString pwmChanDir(int ch) const; // /sys/class/pwm/pwmchip0/pwm0
};

/* 生成一份默认 config.ini 内容(首次运行时写一份给用户改) */
QString defaultConfigText();

/* ============================================================
 * 底层 /sys 节点读写工具
 * ============================================================ */
bool sysfsWrite(const QString &path, const QString &data, QString *err = 0);
bool sysfsRead (const QString &path, QString *out,  QString *err = 0);

/* ============================================================
 * 控制函数
 *  simulation=true 时只"模拟", 返回 true, 方便 PC 演示;
 *  simulation=false 时通过 /sys/class/gpio、/sys/class/pwm 真实控制
 * ============================================================ */

/* 灯开关: 直接置 GPIO 高低电平(on=true 开灯; activeHigh 决定高/低点亮) */
bool gpioSetPin(bool simulation, int gpio, bool on, bool activeHigh, QString *err = 0);

/* PWM: 设置周期/占空(纳秒), 以及输出使能 */
bool pwmSetDuty   (bool simulation, int chip, int ch, int periodNs, int dutyNs, QString *err = 0);
bool pwmSetEnabled(bool simulation, int chip, int ch, bool en, QString *err = 0);

/* 读 ADC 原始值 */
bool adcReadRaw(const QString &path, int *out, QString *err = 0);

/* ============================================================
 * DS18B20 温度读取
 * ============================================================ */

/* 从内核节点读温度(支持 w1_slave "…t=12345" 与普通 "23.5" 两种格式) */
bool readTemperatureFromNode(const QString &path, double *out, QString *err = 0);

/* 用户空间 1-Wire 位带时序(不依赖内核驱动, 直接驱动 GPX0_6)
 * 建议调用流程(转换需 750ms, 不要阻塞界面):
 *   begin() -> convertRequest() -> 等待约 800ms -> readTemperature()
 * 时序为微秒级, 板载运行偶有抖动属正常, 更稳的方案是把 source 配成 node */
class Ds18b20
{
public:
    Ds18b20();
    ~Ds18b20();

    /* export GPX0_6 并置为输出高(空闲态). 返回是否成功 */
    bool begin(int gpio, QString *err = 0);
    bool isReady() const { return m_gpio >= 0; }

    /* 复位+跳过ROM+发起温度转换(异步, 之后等待约 800ms) */
    bool convertRequest(QString *err = 0);

    /* 复位+读 9 字节暂存器, 解析温度(单位 ℃) */
    bool readTemperature(double *out, QString *err = 0);

private:
    int m_gpio;

    bool  setDir(const QString &dir, QString *err) const;       // 写 direction
    bool  gpioOut(int v, QString *err) const;                   // 写 value(输出)
    bool  gpioIn(int *v, QString *err) const;                   // 读 value(输入)
    bool  reset(QString *err) const;                            // 复位+存在检测
    void  writeBit(int b) const;
    int   readBit() const;
    void  writeByte(int b) const;
    int   readByte() const;
};

#endif // HARDWARE_H
