#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QStringList>
#include <QTime>
#include <QByteArray>

#include "hardware.h"
#include "feedschedule.h"

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
class QTimeEdit;
class QListWidget;

// 宠物喂食提醒上位机：食盆余量、LED 开关、音乐与温度报警。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void onAddSchedule();
    void onDeleteSchedule();
    void onClearSchedule();
    void onScheduleTick();
    void onLightToggled(bool on);      // 开关灯按钮
    void onSpinChanged(int value);     // 模拟余量输入 -> 同步滑条
    void onSliderChanged(int value);   // 滑条变化 -> 同步输入框
    void onApplyFoodLevel();                 // 应用模拟食盆余量
    void onPlayClicked();              // 播放声音
    void onFeedClicked();              // 手动喂食(立即喂食按钮)

    void onMelodyTick();               // 旋律定时步进(门铃/警报通用)
    void onAlarmBlink();               // 报警灯闪烁

    void onTempPoll();                 // 定时读 DS18B20
    void onThresholdChanged(double v); // 阈值改动立即重新判断
    void onAlarmToggle(bool checked);  // 高温报警 开/关

    void pollAdc();                    // 定时读 ADC
    void onAutoToggled(bool checked);  // 余量不足提醒
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
    void syncFoodLevelUi(int percent);
    void refreshFoodLevel();

    // ---- 温度 / 报警 ----
    void handleTemp(double t, bool valid);       // 更新界面并按阈值判断报警
    void evaluateAlarm();                        // 阈值重判
    void startAlarm();                           // 进入报警
    void stopAlarm();                            // 解除报警
    void startAlarmMelody();                     // 循环警报音
    void stopMelody();                           // 停旋律并关蜂鸣器
    void setTone(int freqHz);                    // 蜂鸣器发声(内部)

    void log(const QString &s);
    void refreshSchedule();
    void saveSchedule();

    QString m_configPath;
    FeedSchedule m_schedule;
    QTimeEdit *m_scheduleTime;
    QListWidget *m_scheduleList;
    QLabel *m_currentTime;
    QLabel *m_scheduleStatus;

    HardwareConfig m_cfg;

    // ---- 灯光状态 ----
    bool m_lightOn;
    int  m_foodPercent;

    // ---- 旋律(门铃/警报共用引擎) ----
    bool m_playing;
    bool m_melodyLoop;        // true=循环播放(警报)
    bool m_beeped;            // 模拟模式只"嘀"一声
    QTimer *m_melodyTimer;
    QVector<int> m_melody;
    int m_melodyIndex;

    QTimer     *m_adcTimer;
    bool        m_auto;   // 余量不足自动提醒

    // ---- 温度 / 报警状态 ----
    bool     m_tempValid;
    double   m_tempNow;
    bool     m_alarmEnabled;   // 高温报警开关
    bool     m_alarming;
    QTimer  *m_tempTimer;
    QTimer  *m_alarmBlink;
    bool     m_blinkOn;

    // ---- 界面部件 ----
    QLabel     *m_stateLabel;   // 提醒灯状态文字
    QPushButton* m_btnLight;
    QSpinBox   *m_spinFoodLevel;
    QSlider    *m_sliderFoodLevel;
    QPushButton* m_btnFoodReminder;
    QPushButton* m_btnPlay;
    QPushButton* m_btnMusic;   // 音乐模块“试听”按钮(喂食版界面)
    QPushButton* m_chkAuto;    // 余量提醒(胶囊按钮)
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
