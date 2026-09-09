#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QStringList>
#include <QTime>
#include <QByteArray>

#include "hardware.h"

class QLabel;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QProgressBar;
class QTextEdit;
class QTimer;
class QTcpServer;
class QTcpSocket;

/* ============================================================
 * 会发光的"灯泡"控件: 根据开关状态与亮度值(QPainter 绘制)
 * ============================================================ */
class BulbWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BulbWidget(QWidget *parent = 0);

    void setState(bool on, int brightness);   // 开/关 与 亮度 0~100
    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);

private:
    bool m_on;
    int  m_bright;
};

/* ============================================================
 * 主窗口: 智能家居 LED + DS18B20 温度控制系统
 *  - 开关灯按钮(GPIO)
 *  - PWM 亮度输入框(QSpinBox, 回车或点"应用"生效)
 *  - 播放按钮(蜂鸣器)
 *  - DS18B20 温度实时显示, 超阈值报警: LED 闪烁 + 警报音乐
 * ============================================================ */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onLightToggled(bool on);      // 开关灯按钮
    void onSpinChanged(int value);     // PWM 输入框变化 -> 同步滑条
    void onSliderChanged(int value);   // 滑条变化 -> 同步输入框
    void onApplyPwm();                 // 应用 PWM 亮度
    void onPlayClicked();              // 播放声音
    void onFeedClicked();              // 手动喂食(立即喂食按钮)

    void onMelodyTick();               // 旋律定时步进(门铃/警报通用)
    void onAlarmBlink();               // 报警灯闪烁

    void onTempPoll();                 // 定时读 DS18B20
    void onTempReadDone();             // 位带方式 750ms 转换完成后的读结果
    void onThresholdChanged(double v); // 阈值改动立即重新判断
    void onAlarmToggle(bool checked);  // 高温报警 开/关

    void pollAdc();                    // 定时读 ADC
    void onAutoToggled(bool checked);  // 智能调光
    void onTcpNewConnection();         // 下位机连接
    void onTcpReadyRead();             // 接收下位机数据
    void onTcpDisconnected();          // 下位机断开

private:
    void buildUi();                    // 搭建界面
    void applyStyle();                 // QSS 美化
    void loadConfig();                 // 读取/生成 config.ini
    void startTcpServer();             // 启动上位机 TCP Server
    void sendCommand(const QString &cmd);
    void handleLowerMessage(const QString &msg);

    // ---- 灯光 ----
    void applyLight(bool on, bool logIt);
    void applyBrightness(int percent, bool logIt);
    void syncPwmUi(int percent);
    void refreshBulb();

    // ---- 温度 / 报警 ----
    void handleTemp(double t, bool valid);       // 更新界面并按阈值判断报警
    void evaluateAlarm();                        // 阈值重判
    void startAlarm();                           // 进入报警
    void stopAlarm();                            // 解除报警
    void startAlarmMelody();                     // 循环警报音
    void stopMelody();                           // 停旋律并关蜂鸣器
    void setTone(int freqHz);                    // 蜂鸣器发声(内部)

    void log(const QString &s);

    HardwareConfig m_cfg;

    // ---- 灯光状态 ----
    bool m_lightOn;
    int  m_pwmPercent;

    // ---- 旋律(门铃/警报共用引擎) ----
    bool m_playing;
    bool m_melodyLoop;        // true=循环播放(警报)
    bool m_beeped;            // 模拟模式只"嘀"一声
    QTimer *m_melodyTimer;
    QVector<int> m_melody;
    int m_melodyIndex;

    QTimer     *m_adcTimer;
    bool        m_auto;   // 智能自动调光

    // ---- 温度 / 报警状态 ----
    Ds18b20 *m_ds;   // 位带方式对象(仅 real+bitbang 时创建)
    bool     m_tempValid;
    double   m_tempNow;
    bool     m_alarmEnabled;   // 高温报警开关
    bool     m_alarming;
    int      m_dsPhase;        // 0=空闲 1=等转换(750ms)
    QTimer  *m_tempTimer;
    QTimer  *m_alarmBlink;
    bool     m_blinkOn;

    // ---- 界面部件 ----
    BulbWidget *m_bulb;
    QLabel     *m_stateLabel;   // 灯泡状态文字
    QLabel     *m_brightLabel;
    QPushButton* m_btnLight;
    QSpinBox   *m_spinPwm;
    QSlider    *m_sliderPwm;
    QPushButton* m_btnApply;
    QPushButton* m_btnPlay;
    QPushButton* m_btnMusic;   // 音乐模块“试听”按钮(喂食版界面)
    QPushButton* m_chkAuto;    // 自动调光(胶囊按钮)
    QLabel     *m_adcValLabel;
    QProgressBar* m_adcBar;
    QProgressBar* m_adcBarSmall; // 喂食版界面: 中卡小型余量条
    QLabel      *m_feedCountL;   // 喂食版界面: 今日喂食次数
    int          m_feedToday;
    QTime        m_lastFeed;
    QLabel     *m_adcTip;

    // ---- 上位机 / 下位机 TCP 通信 ----
    QTcpServer *m_tcpServer;
    QTcpSocket *m_client;
    QByteArray  m_rxBuffer;

    // 温度卡
    QLabel       *m_tempVal;  // 当前温度大字
    QLabel       *m_tempSrcTip;  // 传感器说明小字
    QDoubleSpinBox* m_spinTemp;  // 报警阈值输入框
    QPushButton  *m_chkAlarm;  // 高温报警 开/关
    QLabel       *m_alarmBanner;  // 高温报警横幅

    QTextEdit  *m_log;
    QLabel     *m_tipLabel;
};

#endif // MAINWINDOW_H
