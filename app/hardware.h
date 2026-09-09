#ifndef HARDWARE_H
#define HARDWARE_H

#include <QString>

// PC 上位机配置；LED、蜂鸣器和传感器由板端 SDK 控制。
struct HardwareConfig
{
    HardwareConfig();
    bool simulation;
    double tempThreshold;
    double tempHysteresis;
    int adcMax;
    bool loadFrom(const QString &file);
};

QString defaultConfigText();

#endif
