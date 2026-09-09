#include "hardware.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QSettings>
#include <QThread>
#include <QCoreApplication>

/* ---------------- 小工具 ---------------- */

bool sysfsWrite(const QString &path, const QString &data, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = QString("%1 打开失败: %2").arg(path, f.errorString());
        return false;
    }
    QTextStream ts(&f);
    ts << data;
    f.close();
    return true;
}

bool sysfsRead(const QString &path, QString *out, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QString("%1 打开失败: %2").arg(path, f.errorString());
        return false;
    }
    QTextStream ts(&f);
    *out = ts.readAll();
    f.close();
    return true;
}

/* ---------------- 配置 ---------------- */

HardwareConfig::HardwareConfig()
    : simulation(true),
      ledGpio(247),
      ledActiveHigh(true),
      ledPwmChannel(-1),
      buzzerPwmChannel(0),
      buzzerFreq(660),
      buzzerDutyPercent(50),
      pwmChip(0),
      tempSource("sim"),
      tempGpio(230),
      tempThreshold(30.0),
      tempHysteresis(2.0),
      adcMax(4095)
{
}

QString HardwareConfig::pwmDir() const
{
    return QString("/sys/class/pwm/pwmchip%1").arg(pwmChip);
}

QString HardwareConfig::pwmChanDir(int ch) const
{
    return QString("%1/pwm%2").arg(pwmDir()).arg(ch);
}

bool HardwareConfig::loadFrom(const QString &file)
{
    QSettings s(file, QSettings::IniFormat);

    s.beginGroup("system");
    simulation = s.value("simulation", simulation ? 1 : 0).toInt() != 0;
    s.endGroup();

    s.beginGroup("led");
    ledGpio        = s.value("gpio",        ledGpio).toInt();
    ledActiveHigh  = s.value("activeHigh",  ledActiveHigh ? 1 : 0).toInt() != 0;
    ledPwmChannel  = s.value("pwmChannel",  ledPwmChannel).toInt();
    s.endGroup();

    s.beginGroup("buzzer");
    buzzerPwmChannel = s.value("pwmChannel", buzzerPwmChannel).toInt();
    buzzerFreq       = s.value("freq",       buzzerFreq).toInt();
    buzzerDutyPercent= s.value("dutyPercent",buzzerDutyPercent).toInt();
    s.endGroup();

    s.beginGroup("pwm");
    pwmChip = s.value("chip", pwmChip).toInt();
    s.endGroup();

    s.beginGroup("temperature");
    tempSource    = s.value("source", tempSource).toString().trimmed().toLower();
    tempNode      = s.value("node", tempNode).toString();
    tempGpio      = s.value("gpio", tempGpio).toInt();
    tempThreshold = s.value("threshold", tempThreshold).toDouble();
    tempHysteresis= s.value("hysteresis", tempHysteresis).toDouble();
    s.endGroup();

    s.beginGroup("alarm");
    const QString leds = s.value("leds", QString()).toString();
    for (const QString &item : leds.split(',')) {
        bool ok = false;
        const int v = item.trimmed().toInt(&ok);
        if (ok && v >= 0)
            alarmLeds.append(v);
    }
    s.endGroup();

    s.beginGroup("adc");
    adcPath = s.value("path", adcPath).toString();
    adcMax  = s.value("max",  adcMax).toInt();
    s.endGroup();

    return QFile::exists(file);
}

bool HardwareConfig::load()
{
    return loadFrom(QDir::current().filePath("config.ini"));
}

QString defaultConfigText()
{
    return QString::fromUtf8(
"; ============================================================\n"
";  FS4412 智能家居 LED + DS18B20 控制系统 - 硬件配置文件\n"
";  放在程序同目录 config.ini, 修改后重启程序生效\n"
";\n"
";  FS4412 板载 LED(高电平点亮): LED2=GPX2_7 LED3=GPX1_0\n"
";                              LED4=GPF3_4 LED5=GPF3_5\n"
";  DS18B20: DQ = GPX0_6 (底板有插座与 4.7K 上拉)\n"
"; ============================================================\n"
"\n"
"[system]\n"
"; 0 = FS4412 真实开发板(读写 /sys 内核节点)\n"
"; 1 = 模拟模式(无开发板时 PC 演示) 默认 1\n"
"simulation=1\n"
"\n"
"[led]\n"
"; 灯的 GPIO 引脚编号(默认 LED2=GPX2_7, 通常为 247):\n"
"; 查询: cat /sys/kernel/debug/gpio\n"
"gpio=247\n"
"; 高电平点亮填 1, 低电平点亮填 0\n"
"activeHigh=1\n"
"; 灯接在 PWM 引脚可填 PWM 通道 0~3 调光; 纯 GPIO 灯填 -1\n"
"pwmChannel=-1\n"
"\n"
"[buzzer]\n"
"; 蜂鸣器的 PWM 引脚(FS4412 板载蜂鸣器接 PWM0 -> 0)\n"
"pwmChannel=0\n"
"; 频率(Hz)与占空比(%)\n"
"freq=660\n"
"dutyPercent=50\n"
"\n"
"[pwm]\n"
"; PWM 控制器编号: /sys/class/pwm/pwmchip<X>\n"
"chip=0\n"
"\n"
"[temperature]\n"
"; 温度来源: sim=PC模拟  node=读内核节点(w1_slave等)\n"
";           bitbang=用户空间1-Wire位带直接读DS18B20\n"
"source=sim\n"
"; node 方式节点路径(内容形如 ...t=12345 或 23.5)\n"
"node=/sys/bus/w1/devices/28-0000041e2a01/w1_slave\n"
"; bitbang 方式 DQ 引脚 GPX0_6(内核编号通常 230)\n"
"gpio=230\n"
"; 报警阈值与回差(℃)\n"
"threshold=30.0\n"
"hysteresis=2.0\n"
"\n"
"[alarm]\n"
"; 报警时闪烁的 LED(逗号分隔), 可留空\n"
"; FS4412 其余 LED: GPX1_0 通常 232; GPF3_4/5 以 debug/gpio 为准\n"
"leds=\n"
"\n"
"[adc]\n"
"; ADC 原始值节点(没有留空)\n"
"path=\n"
"max=4095\n");
}

/* ---------------- GPIO 灯 ---------------- */

bool gpioSetPin(bool simulation, int gpio, bool on, bool activeHigh, QString *err)
{
    // 电平: 高=1 低=0
    const int value = on ? (activeHigh ? 1 : 0) : (activeHigh ? 0 : 1);

    if (simulation) {
        if (err) *err = QString("模拟模式: 灯 GPIO%1 置 %2(%3)")
                        .arg(gpio).arg(value).arg(on ? "开" : "关");
        return true;
    }

    const QString gpioDir = QString("/sys/class/gpio/gpio%1").arg(gpio);

    if (!QDir(gpioDir).exists()) {
        if (!sysfsWrite("/sys/class/gpio/export", QString::number(gpio), err))
            return false;
        for (int i = 0; i < 30 && !QDir(gpioDir).exists(); ++i)
            QThread::msleep(10);
    }
    if (!sysfsWrite(gpioDir + "/direction", "out", err))
        return false;
    if (!sysfsWrite(gpioDir + "/value", QString::number(value), err))
        return false;

    if (err) *err = QString("灯已%1: GPIO%2 = %3")
                    .arg(on ? "打开" : "关闭").arg(gpio).arg(value);
    return true;
}

/* ---------------- PWM ---------------- */

static bool ensurePwmExported(int chip, int ch, QString *err)
{
    const QString dir = QString("/sys/class/pwm/pwmchip%1/pwm%2").arg(chip).arg(ch);
    if (QDir(dir).exists())
        return true;
    if (!sysfsWrite(QString("/sys/class/pwm/pwmchip%1/export").arg(chip),
                    QString::number(ch), err))
        return false;
    for (int i = 0; i < 30 && !QDir(dir).exists(); ++i)
        QThread::msleep(10);
    return QDir(dir).exists();
}

bool pwmSetDuty(bool simulation, int chip, int ch, int periodNs, int dutyNs, QString *err)
{
    if (simulation) {
        if (err) *err = QString("模拟模式: PWM%1 周期=%2ns 占空=%3ns(%4%)")
                        .arg(ch).arg(periodNs).arg(dutyNs)
                        .arg(periodNs ? int(dutyNs * 100.0 / periodNs) : 0);
        return true;
    }

    if (!ensurePwmExported(chip, ch, err))
        return false;

    const QString dir = QString("/sys/class/pwm/pwmchip%1/pwm%2").arg(chip).arg(ch);

    if (!sysfsWrite(dir + "/period", QString::number(periodNs), err))
        return false;

    QString written;
    if (QFile::exists(dir + "/duty_cycle"))
        written = dir + "/duty_cycle";
    else if (QFile::exists(dir + "/duty"))
        written = dir + "/duty";
    if (!written.isEmpty()) {
        if (!sysfsWrite(written, QString::number(dutyNs), err))
            return false;
    } else {
        if (err) *err = dir + " 下找不到 duty_cycle/duty 节点";
        return false;
    }
    return true;
}

bool pwmSetEnabled(bool simulation, int chip, int ch, bool en, QString *err)
{
    if (simulation) {
        if (err) *err = QString("模拟模式: PWM%1 输出%2")
                        .arg(ch).arg(en ? "使能" : "关闭");
        return true;
    }

    if (!ensurePwmExported(chip, ch, err))
        return false;
    return sysfsWrite(QString("/sys/class/pwm/pwmchip%1/pwm%2/enable")
                          .arg(chip).arg(ch),
                      en ? "1" : "0", err);
}

/* ---------------- ADC ---------------- */

bool adcReadRaw(const QString &path, int *out, QString *err)
{
    if (path.trimmed().isEmpty()) {
        if (err) *err = "未配置 ADC 节点(见 config.ini [adc])";
        return false;
    }
    QString s;
    if (!sysfsRead(path, &s, err))
        return false;
    bool ok = false;
    const int v = s.trimmed().toInt(&ok);
    if (!ok) {
        if (err) *err = QString("%1 内容不是整数: %2").arg(path, s.trimmed());
        return false;
    }
    if (out) *out = v;
    return true;
}

/* ---------------- 温度: 内核节点 ---------------- */

bool readTemperatureFromNode(const QString &path, double *out, QString *err)
{
    QString s;
    if (!sysfsRead(path, &s, err))
        return false;

    bool ok = false;
    double t = 0;

    // w1_slave 格式: "... t=23125" (单位 0.001℃)
    const int pos = s.lastIndexOf("t=");
    if (pos >= 0) {
        QString tail = s.mid(pos + 2).trimmed();
        int space = tail.indexOf(' ');
        if (space > 0)
            tail = tail.left(space);
        bool good = false;
        const qint64 v = tail.toLongLong(&good);
        if (good) { t = v / 1000.0; ok = true; }
    }

    // 普通格式: "23.5\n"
    if (!ok) {
        t = s.trimmed().toDouble(&ok);
    }

    if (!ok) {
        if (err) *err = QString("%1 内容无法解析为温度: %2").arg(path, s.trimmed().left(40));
        return false;
    }
    if (out) *out = t;
    return true;
}

/* ---------------- 温度: 用户空间 1-Wire 位带 ---------------- */

Ds18b20::Ds18b20()
    : m_gpio(-1)
{
}

Ds18b20::~Ds18b20()
{
    // 结束前把总线释放为高电平, 不影响后续使用
    if (m_gpio >= 0)
        gpioOut(1, 0);
}

bool Ds18b20::begin(int gpio, QString *err)
{
    const QString gpioDir = QString("/sys/class/gpio/gpio%1").arg(gpio);
    if (!QDir(gpioDir).exists()) {
        if (!sysfsWrite("/sys/class/gpio/export", QString::number(gpio), err))
            return false;
        for (int i = 0; i < 30 && !QDir(gpioDir).exists(); ++i)
            QThread::msleep(10);
    }
    if (!QDir(gpioDir).exists()) {
        if (err) *err = QString("GPIO%1 export 失败").arg(gpio);
        return false;
    }
    m_gpio = gpio;
    setDir("out", err);
    gpioOut(1, 0);              // 空闲态: 总线释放(上拉为高)
    return true;
}

bool Ds18b20::setDir(const QString &dir, QString *err) const
{
    return sysfsWrite(QString("/sys/class/gpio/gpio%1/direction").arg(m_gpio), dir, err);
}

bool Ds18b20::gpioOut(int v, QString *err) const
{
    setDir("out", 0);
    return sysfsWrite(QString("/sys/class/gpio/gpio%1/value").arg(m_gpio),
                      v ? "1" : "0", err);
}

bool Ds18b20::gpioIn(int *v, QString *err) const
{
    setDir("in", 0);
    QString s;
    if (!sysfsRead(QString("/sys/class/gpio/gpio%1/value").arg(m_gpio), &s, err))
        return false;
    *v = s.trimmed() == "1";
    return true;
}

static void dsDelay(int us)
{
    // 微秒级延时(Qt 内部用 nanosleep/usleep 实现)
    if (us > 0)
        QThread::usleep(ulong(us));
}

/* 复位脉冲 + 存在检测; 返回 true 表示检测到 DS18B20(存在脉冲拉低) */
bool Ds18b20::reset(QString *err) const
{
    Q_UNUSED(err);
    gpioOut(0, 0);              // 拉低 >=480us
    dsDelay(520);
    gpioOut(1, 0);              // 释放
    dsDelay(70);
    int p = 1;
    gpioIn(&p, 0);              // 采样: 器件存在会拉低 ~60us
    dsDelay(430);
    return p == 0;
}

void Ds18b20::writeBit(int b) const
{
    if (b) {
        gpioOut(0, 0); dsDelay(6);
        gpioOut(1, 0); dsDelay(64);     // 写1: 短拉低即释放
    } else {
        gpioOut(0, 0); dsDelay(60);
        gpioOut(1, 0); dsDelay(8);      // 写0: 持续拉低
    }
}

int Ds18b20::readBit() const
{
    gpioOut(0, 0); dsDelay(3);
    gpioOut(1, 0); dsDelay(8);
    int v = 1;
    gpioIn(&v, 0);                      // 采样窗口 ~15us
    dsDelay(55);
    return v;
}

void Ds18b20::writeByte(int b) const
{
    for (int i = 0; i < 8; ++i) {
        writeBit(b & 1);
        b >>= 1;
    }
}

int Ds18b20::readByte() const
{
    int b = 0;
    for (int i = 0; i < 8; ++i)
        b |= readBit() << i;
    return b;
}

bool Ds18b20::convertRequest(QString *err)
{
    if (!reset(err)) {
        if (err) *err = "未检测到 DS18B20(请确认已插入插座, 引脚为 GPX0_6)";
        return false;
    }
    writeByte(0xCC);   // Skip ROM
    writeByte(0x44);   // Convert T (之后需等待约 750ms)
    return true;
}

bool Ds18b20::readTemperature(double *out, QString *err)
{
    if (!reset(err)) {
        if (err) *err = "未检测到 DS18B20(请确认已插入插座, 引脚为 GPX0_6)";
        return false;
    }
    writeByte(0xCC);   // Skip ROM
    writeByte(0xBE);   // Read Scratchpad

    const int lsb = readByte();
    const int msb = readByte();
    for (int i = 0; i < 7; ++i) readByte();   // 丢弃其余字节(CRC 未校验)

    const short raw = short(lsb | (msb << 8));
    double t = raw * 0.0625;                  // 12 位分辨率 0.0625℃
    if (out) *out = t;
    return true;
}

