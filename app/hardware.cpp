#include "hardware.h"
#include <QFile>
#include <QSettings>

HardwareConfig::HardwareConfig()
    : simulation(true), tempThreshold(30.0), tempHysteresis(2.0), alarmSoundSeconds(5), adcMax(4095)
{
}

bool HardwareConfig::loadFrom(const QString &file)
{
    QSettings s(file, QSettings::IniFormat);
    simulation = s.value("system/simulation", simulation ? 1 : 0).toInt() != 0;
    tempThreshold = s.value("temperature/threshold", tempThreshold).toDouble();
    tempHysteresis = qMax(0.0, s.value("temperature/hysteresis", tempHysteresis).toDouble());
    alarmSoundSeconds = qMax(1, s.value("alarm/sound_seconds", alarmSoundSeconds).toInt());
    adcMax = qMax(1, s.value("adc/max", adcMax).toInt());
    return QFile::exists(file);
}

QString defaultConfigText()
{
    return QString::fromUtf8(
        "; FS4412 宠物喂食提醒控制器\n"
        "; 放在程序同目录，修改后重启生效。硬件由板端 SDK 控制。\n"
        "\n"
        "[system]\n"
        "; 1 = PC 模拟演示，0 = 连接真实开发板\n"
        "simulation=1\n"
        "\n"
        "[temperature]\n"
        "; 高温报警阈值与回差（摄氏度），真实温度由 TCP 上报\n"
        "threshold=30.0\n"
        "hysteresis=2.0\n"
        "\n"
        "[alarm]\n"
        "; 高温报警时蜂鸣器持续响多久（秒）\n"
        "sound_seconds=5\n"
        "\n"
        "[adc]\n"
        "; ADC 满量程，用于余量显示换算\n"
        "max=4095\n");
}
