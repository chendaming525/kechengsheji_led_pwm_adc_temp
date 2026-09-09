#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QStyle>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QProgressBar>
#include <QTextEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QTimeEdit>
#include <QListWidget>
#include <QSettings>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QPainter>
#include <QRadialGradient>
#include <QPolygon>

#include <cmath>

class SignalBlocker
{
public:
    explicit SignalBlocker(QObject *obj)
        : m_obj(obj), m_wasBlocked(obj ? obj->signalsBlocked() : false)
    {
        if (m_obj)
            m_obj->blockSignals(true);
    }

    ~SignalBlocker()
    {
        if (m_obj)
            m_obj->blockSignals(m_wasBlocked);
    }

private:
    Q_DISABLE_COPY(SignalBlocker)
    QObject *m_obj;
    bool m_wasBlocked;
};

static QString htmlEscape(QString s)
{
    s.replace("&", "&amp;");
    s.replace("<", "&lt;");
    s.replace(">", "&gt;");
    s.replace("\"", "&quot;");
    s.replace("'", "&#39;");
    return s;
}

/* ============================================================
 * 启动调试日志: 同时输出到 Qt Creator 应用程序输出(qDebug)
 * 和 exe 运行目录的 startup_debug.log(崩溃也能看到最后一步)
 * ============================================================ */
static void dbg(const QString &s)
{
    qDebug() << "[startup]" << s;
    QFile f(QDir::current().filePath("startup_debug.log"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write((QDateTime::currentDateTime().toString("HH:mm:ss.zzz ")
                 + s + "\n").toUtf8());
        f.close();
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_lightOn(false),
      m_foodPercent(50),
      m_playing(false),
      m_melodyLoop(false),
      m_beeped(false),
      m_melodyTimer(0),
      m_melodyIndex(0),
      m_adcTimer(0),
      m_auto(false),
      m_tempValid(false),
      m_tempNow(0),
      m_alarmEnabled(true),
      m_alarming(false),
      m_tempTimer(0),
      m_alarmBlink(0),
      m_blinkOn(false),
      m_stateLabel(0),
      m_btnLight(0),
      m_spinFoodLevel(0),
      m_sliderFoodLevel(0),
      m_btnFoodReminder(0),
      m_btnPlay(0),
      m_btnMusic(0),
      m_chkAuto(0),
      m_adcValLabel(0),
      m_adcBar(0),
      m_adcBarSmall(0),
      m_feedCountL(0),
      m_feedToday(0),
      m_adcTip(0),
      m_tcpServer(0),
      m_client(0),
      m_tempVal(0),
      m_tempSrcTip(0),
      m_spinTemp(0),
      m_chkAlarm(0),
      m_alarmBanner(0),
      m_log(0),
      m_tipLabel(0)
{
    dbg("=== 构造函数开始 ===");
    loadConfig();        dbg("1. loadConfig 完成");
    buildUi();           dbg("2. buildUi 完成");
    applyStyle();
    startTcpServer();    dbg("3. startTcpServer 完成");

    // 旋律: "叮——咚"门铃声(880Hz 两拍, 停顿, 659Hz 两拍)
    m_melody << 880 << 880 << 880 << 880 << 0 << 0
             << 659 << 659 << 659 << 659 << 0 << 0 << 0 << 0;

    QTimer *scheduleTimer = new QTimer(this);
    connect(scheduleTimer, SIGNAL(timeout()), this, SLOT(onScheduleTick()));
    scheduleTimer->start(1000);
    onScheduleTick();
}

MainWindow::~MainWindow()
{
}

/* ---------------- 配置 ---------------- */

void MainWindow::loadConfig()
{
    // 依次找: 当前目录 -> 可执行文件目录
    QString ini = QDir::current().filePath("config.ini");
    if (!QFile::exists(ini))
        ini = QCoreApplication::applicationDirPath() + "/config.ini";
    m_configPath = ini;
    QSettings settings(ini, QSettings::IniFormat);
    foreach (const QString &time, settings.value("schedule/times").toStringList())
        m_schedule.add(time);

    if (!m_cfg.loadFrom(ini)) {
        // 没有配置文件就生成一份默认的, 方便第一次运行
        QFile f(ini);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        f.write(defaultConfigText().toUtf8());
        f.close();
        m_cfg.loadFrom(ini);
    }
}

/* ---------------- 界面搭建 ---------------- */

static QLabel *mkLabel(const QString &text, const char *objName)
{
    QLabel *l = new QLabel(text);
    l->setObjectName(objName);
    l->setWordWrap(true);
    return l;
}

void MainWindow::buildUi()
{
    dbg("buildUi: 设置窗口");
    setWindowTitle("宠物喂食提醒控制器");
    resize(1180, 860);
    setMinimumSize(1020, 760);

    QWidget *root = new QWidget(this);
    root->setObjectName("root");
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(root);
    setCentralWidget(scroll);

    auto *rootLay = new QVBoxLayout(root);
    rootLay->setSizeConstraint(QLayout::SetMinimumSize);
    rootLay->setContentsMargins(24, 18, 24, 16);
    rootLay->setSpacing(14);

    auto *title = mkLabel("宠物喂食提醒控制器", "appTitle");
    title->setAlignment(Qt::AlignCenter);
    rootLay->addWidget(title);

    auto *statusRow = new QHBoxLayout();
    statusRow->addWidget(mkLabel("食盆余量监测(ADC)", "statusText"));
    statusRow->addWidget(mkLabel("余量不足灯光提醒", "statusText"));
    statusRow->addWidget(mkLabel("定时喂食", "statusText"));
    statusRow->addWidget(mkLabel("到点播放提示音乐(PWM)", "statusText"));
    statusRow->addStretch();
    rootLay->addLayout(statusRow);

    auto *mainGrid = new QGridLayout();
    mainGrid->setSpacing(16);
    mainGrid->setColumnStretch(0, 1);
    mainGrid->setColumnStretch(1, 1);
    mainGrid->setColumnStretch(2, 1);

    /* 左侧：食盆余量 */
    auto *leftCard = new QFrame();
    leftCard->setObjectName("card");
    auto *leftLay = new QVBoxLayout(leftCard);
    leftLay->setSizeConstraint(QLayout::SetMinimumSize);
    leftLay->setContentsMargins(18, 16, 18, 16);
    leftLay->setSpacing(14);

    auto *leftTitle = mkLabel("食盆 · 余量显示", "sectionTitle");
    leftLay->addWidget(leftTitle);

    auto *tank = new QFrame();
    tank->setObjectName("foodTank");
    tank->setMinimumHeight(180);
    tank->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *tankLay = new QVBoxLayout(tank);
    tankLay->setSizeConstraint(QLayout::SetMinimumSize);
    tankLay->setContentsMargins(18, 18, 18, 18);
    tankLay->setSpacing(8);

    m_adcBar = new QProgressBar();
    m_adcBar->setObjectName("adcBar");
    m_adcBar->setRange(0, 100);
    m_adcBar->setValue(62);
    m_adcBar->setTextVisible(false);
    m_adcBar->setAlignment(Qt::AlignCenter);
    m_adcBar->setMinimumHeight(64);
    m_adcBar->setMaximumHeight(100);
    m_adcBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //m_adcBar->setStyleSheet("QProgressBar#adcBar { border: 0; background: rgba(255,255,255,0.08); border-radius: 22px; } QProgressBar#adcBar::chunk { background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #d9a34a, stop:1 #b97819); border-radius: 22px; }");
    tankLay->addWidget(m_adcBar);

    m_adcValLabel = mkLabel("62%", "tankPercent");
    m_adcValLabel->setWordWrap(false);
    m_adcValLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_adcValLabel->setAlignment(Qt::AlignCenter);
    tankLay->addWidget(m_adcValLabel);
    leftLay->addWidget(tank);

    m_btnLight = new QPushButton("打开提醒灯");
    m_btnLight->setObjectName("btnLight");
    m_btnLight->setCheckable(true);
    m_btnLight->setCursor(Qt::PointingHandCursor);
    leftLay->addWidget(m_btnLight);

    m_stateLabel = mkLabel("● 余量充足", "stateLabel");
    m_stateLabel->setAlignment(Qt::AlignCenter);
    leftLay->addWidget(m_stateLabel);
    leftLay->addWidget(mkLabel("提醒灯状态：正常", "hintLabel"));

    mainGrid->addWidget(leftCard, 0, 0);
    dbg("buildUi: 左卡(食盆)完成");

    /* 中间：ADC 余量检测 */
    auto *midCard = new QFrame();
    midCard->setObjectName("card");
    auto *midLay = new QVBoxLayout(midCard);
    midLay->setContentsMargins(18, 16, 18, 16);
    midLay->setSpacing(14);

    auto *midTitle = mkLabel("食盆余量 · ADC (电位器)", "sectionTitle");
    midLay->addWidget(midTitle);
    auto *adcHead = new QHBoxLayout();
    m_adcTip = mkLabel("食盆余量检测 AIN3", "chip");
    adcHead->addWidget(m_adcTip);
    adcHead->addStretch();
    midLay->addLayout(adcHead);

    m_adcBarSmall = new QProgressBar();
    m_adcBarSmall->setObjectName("adcBarSmall");
    m_adcBarSmall->setFixedHeight(18);
    m_adcBarSmall->setRange(0, 100);
    m_adcBarSmall->setValue(62);
    m_adcBarSmall->setTextVisible(false);
    midLay->addWidget(m_adcBarSmall);

    auto *adcSliderRow = new QHBoxLayout();
    auto *adcLabel = mkLabel("模拟电位器", "ctlTitle");
    adcSliderRow->addWidget(adcLabel);
    m_spinFoodLevel = new QSpinBox();
    m_spinFoodLevel->setObjectName("spinFoodLevel");
    m_spinFoodLevel->setRange(0, 100);
    m_spinFoodLevel->setValue(62);
    m_spinFoodLevel->setSuffix(" %");
    m_spinFoodLevel->setMinimumWidth(90);
    adcSliderRow->addWidget(m_spinFoodLevel);
    m_sliderFoodLevel = new QSlider(Qt::Horizontal);
    m_sliderFoodLevel->setObjectName("sliderFoodLevel");
    m_sliderFoodLevel->setRange(0, 100);
    m_sliderFoodLevel->setValue(62);
    adcSliderRow->addWidget(m_sliderFoodLevel, 1);
    midLay->addLayout(adcSliderRow);

    auto *adcHint = mkLabel("模拟电位器: raw=0x100/4095，电压 = 1.8V", "hintLabel");
    midLay->addWidget(adcHint);

    auto *adcLow = new QHBoxLayout();
    m_chkAuto = new QPushButton("余量不足提醒");
    m_chkAuto->setObjectName("chkAuto");
    m_chkAuto->setCheckable(true);
    m_chkAuto->setChecked(true);
    m_auto = true;    // 默认开启: 余量 ≤20% 自动点亮提醒灯
    adcLow->addWidget(m_chkAuto);
    m_btnFoodReminder = new QPushButton("余量提醒");
    m_btnFoodReminder->setObjectName("btnFoodReminder");
    adcLow->addWidget(m_btnFoodReminder);
    midLay->addLayout(adcLow);

    mainGrid->addWidget(midCard, 0, 1);
    dbg("buildUi: 中卡(ADC)完成");

    /* 右侧：手动喂食 */
    auto *rightCard = new QFrame();
    rightCard->setObjectName("card");
    auto *rightLay = new QVBoxLayout(rightCard);
    rightLay->setContentsMargins(18, 16, 18, 16);
    rightLay->setSpacing(14);

    auto *rightTitle = mkLabel("手动喂食", "sectionTitle");
    rightLay->addWidget(rightTitle);

    m_btnPlay = new QPushButton("立即喂食");
    m_btnPlay->setObjectName("btnPlay");
    m_btnPlay->setCheckable(false);
    m_btnPlay->setMinimumHeight(64);
    rightLay->addWidget(m_btnPlay);

    m_feedCountL = mkLabel("今日喂食 0 次", "feedCount");
    m_feedCountL->setAlignment(Qt::AlignCenter);
    rightLay->addWidget(m_feedCountL);

    auto *feedHint = mkLabel("根据剩余量和提醒时间，按需手动补充食物。", "hintLabel");
    feedHint->setWordWrap(true);
    rightLay->addWidget(feedHint);
    mainGrid->addWidget(rightCard, 0, 2);
    dbg("buildUi: 右卡(手动喂食)完成");

    rootLay->addLayout(mainGrid, 1);

    /* 下层：定时喂食 + 音乐模块 */
    auto *bottomGrid = new QGridLayout();
    bottomGrid->setSpacing(16);
    bottomGrid->setColumnStretch(0, 1);
    bottomGrid->setColumnStretch(1, 1);

    auto *scheduleCard = new QFrame();
    scheduleCard->setObjectName("card");
    auto *scheduleLay = new QVBoxLayout(scheduleCard);
    scheduleLay->setContentsMargins(18, 16, 18, 16);
    scheduleLay->setSpacing(12);

    auto *scheduleTitle = mkLabel("定时喂食", "sectionTitle");
    scheduleLay->addWidget(scheduleTitle);

    auto *timeRow = new QHBoxLayout();
    m_scheduleTime = new QTimeEdit(QTime::currentTime().addSecs(60));
    m_scheduleTime->setObjectName("scheduleTime");
    m_scheduleTime->setDisplayFormat("HH:mm");
    m_scheduleTime->setMinimumWidth(110);
    timeRow->addWidget(m_scheduleTime);
    timeRow->addStretch();
    m_currentTime = mkLabel("", "chip");
    timeRow->addWidget(m_currentTime);
    scheduleLay->addLayout(timeRow);

    m_scheduleList = new QListWidget();
    m_scheduleList->setObjectName("scheduleList");
    m_scheduleList->setMaximumHeight(80);
    scheduleLay->addWidget(m_scheduleList);

    auto *timeButtons = new QHBoxLayout();
    auto *addBtn = new QPushButton("+ 添加");
    auto *delBtn = new QPushButton("删除");
    auto *clearBtn = new QPushButton("清空");
    timeButtons->addWidget(addBtn);
    timeButtons->addWidget(delBtn);
    timeButtons->addWidget(clearBtn);
    scheduleLay->addLayout(timeButtons);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddSchedule()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onDeleteSchedule()));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearSchedule()));

    auto *scheduleState = new QHBoxLayout();
    m_scheduleStatus = mkLabel("", "statusChip");
    scheduleState->addWidget(m_scheduleStatus);
    scheduleState->addWidget(mkLabel("每天到点播放音乐", "hintLabel"));
    scheduleLay->addLayout(scheduleState);
    refreshSchedule();

    bottomGrid->addWidget(scheduleCard, 0, 0);

    auto *musicCard = new QFrame();
    musicCard->setObjectName("card");
    auto *musicLay = new QVBoxLayout(musicCard);
    musicLay->setContentsMargins(18, 16, 18, 16);
    musicLay->setSpacing(12);

    auto *musicTitle = mkLabel("音乐模块 · PWM", "sectionTitle");
    musicLay->addWidget(musicTitle);

    m_btnMusic = new QPushButton("试听喂食提示音乐");
    m_btnMusic->setObjectName("btnPlay");
    m_btnMusic->setMinimumHeight(52);
    musicLay->addWidget(m_btnMusic);

    auto *musicState = mkLabel("空闲", "statusChip");
    musicLay->addWidget(musicState);

    auto *musicHint = mkLabel("播放为“叮咚”提示音，提醒宠物按时进食。", "hintLabel");
    musicHint->setWordWrap(true);
    musicLay->addWidget(musicHint);

    bottomGrid->addWidget(musicCard, 0, 1);
    dbg("buildUi: 底部(定时/音乐)完成");
    rootLay->addLayout(bottomGrid, 1);

    m_log = new QTextEdit();
    m_log->setObjectName("logBox");
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(120);
    rootLay->addWidget(m_log);

    m_tipLabel = mkLabel("宠物喂食提醒控制器 · 食盆余量检测、LED 提醒与音乐提示", "tipLabel");
    m_tipLabel->setAlignment(Qt::AlignCenter);
    rootLay->addWidget(m_tipLabel);

    /* ---------- 信号连接 ---------- */
    connect(m_btnLight, SIGNAL(toggled(bool)), this, SLOT(onLightToggled(bool)));
    connect(m_spinFoodLevel, SIGNAL(valueChanged(int)), this, SLOT(onSpinChanged(int)));
    connect(m_sliderFoodLevel, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));
    connect(m_btnFoodReminder, SIGNAL(clicked()), this, SLOT(onApplyFoodLevel()));
    connect(m_spinFoodLevel, SIGNAL(editingFinished()), this, SLOT(onApplyFoodLevel()));
    connect(m_sliderFoodLevel, SIGNAL(sliderReleased()), this, SLOT(onApplyFoodLevel()));
    connect(m_btnPlay, SIGNAL(clicked()), this, SLOT(onFeedClicked())); // 立即喂食
    connect(m_btnMusic, SIGNAL(clicked()), this, SLOT(onPlayClicked())); // 试听音乐
    if (m_chkAuto)
        connect(m_chkAuto, SIGNAL(toggled(bool)), this, SLOT(onAutoToggled(bool)));
    if (m_chkAlarm)
        connect(m_chkAlarm, SIGNAL(toggled(bool)), this, SLOT(onAlarmToggle(bool)));
    if (m_spinTemp)
        connect(m_spinTemp, SIGNAL(valueChanged(double)), this, SLOT(onThresholdChanged(double)));
    m_spinFoodLevel->setEnabled(m_cfg.simulation);
    m_sliderFoodLevel->setEnabled(m_cfg.simulation);
    m_btnFoodReminder->setEnabled(m_cfg.simulation);
    dbg("buildUi: 信号连接完成");

    m_adcTimer = new QTimer(this);
    m_adcTimer->setInterval(600);
    connect(m_adcTimer, SIGNAL(timeout()), this, SLOT(pollAdc()));
    m_adcTimer->start();

    m_tempTimer = new QTimer(this);
    m_tempTimer->setInterval(1500);
    connect(m_tempTimer, SIGNAL(timeout()), this, SLOT(onTempPoll()));
    if (m_tempVal)            // 喂食版界面没有温度卡片, 不轮询温度(否则会误触发高温报警崩溃)
        m_tempTimer->start();

    m_alarmBlink = new QTimer(this);
    m_alarmBlink->setInterval(500);
    connect(m_alarmBlink, SIGNAL(timeout()), this, SLOT(onAlarmBlink()));

    m_melodyTimer = new QTimer(this);
    m_melodyTimer->setInterval(140);
    connect(m_melodyTimer, SIGNAL(timeout()), this, SLOT(onMelodyTick()));

    dbg("buildUi: 定时器完成, 即将调用 refreshFoodLevel");
    refreshFoodLevel();
    dbg("buildUi: 余量显示更新完成");
}

void MainWindow::refreshSchedule()
{
    m_scheduleList->clear();
    m_scheduleList->addItems(m_schedule.times);
    if (m_scheduleList->count())
        m_scheduleList->setCurrentRow(0);
    m_scheduleStatus->setText(m_schedule.times.isEmpty()
        ? "未设置定时提醒" : QString("已启用 %1 个提醒").arg(m_schedule.times.size()));
}

void MainWindow::saveSchedule()
{
    QSettings settings(m_configPath, QSettings::IniFormat);
    settings.setValue("schedule/times", m_schedule.times);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        log("<b>[定时喂食]</b> 保存失败，请检查 config.ini 是否可写");
    refreshSchedule();
}

void MainWindow::onAddSchedule()
{
    const QString time = m_scheduleTime->time().toString("HH:mm");
    if (!m_schedule.add(time)) {
        log(QString("<b>[定时喂食]</b> %1 已存在").arg(time));
        return;
    }
    saveSchedule();
    log(QString("<b>[定时喂食]</b> 已添加每天 %1 的音乐提醒").arg(time));
}

void MainWindow::onDeleteSchedule()
{
    QListWidgetItem *item = m_scheduleList->currentItem();
    if (!item) {
        log("<b>[定时喂食]</b> 请先选择要删除的时间");
        return;
    }
    const QString time = item->text();
    m_schedule.times.removeAll(time);
    saveSchedule();
    log(QString("<b>[定时喂食]</b> 已删除 %1").arg(time));
}

void MainWindow::onClearSchedule()
{
    m_schedule.times.clear();
    saveSchedule();
    log("<b>[定时喂食]</b> 已清空全部提醒");
}

void MainWindow::onScheduleTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_currentTime->setText("当前 " + now.toString("HH:mm:ss"));
    foreach (const QString &time, m_schedule.takeDue(now)) {
        if (!m_cfg.simulation && (!m_client || m_client->state() != QAbstractSocket::ConnectedState)) {
            log(QString("<b>[定时喂食]</b> %1 已到，下位机未连接，无法播放音乐").arg(time));
            continue;
        }
        log(QString("<b>[定时喂食]</b> %1 已到，触发喂食音乐").arg(time));
        onPlayClicked();
    }
}

/* ---------------- 上位机 / 下位机 TCP 通信 ---------------- */

void MainWindow::startTcpServer()
{
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onTcpNewConnection()));

    if (!m_tcpServer->listen(QHostAddress::Any, 8888)) {
        log(QString("<font color='#ff6b6b'><b>[TCP]</b> 监听 8888 失败: %1</font>")
                .arg(m_tcpServer->errorString()));
        return;
    }
    log("<b>[TCP]</b> 上位机服务已启动, 监听端口 8888, 等待 FS4412 下位机连接");
}

void MainWindow::onTcpNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *s = m_tcpServer->nextPendingConnection();
        if (m_client) {
            s->disconnectFromHost();
            s->deleteLater();
            log("<font color='#ffb86c'><b>[TCP]</b> 已有下位机连接, 拒绝新的连接</font>");
            continue;
        }

        m_client = s;
        m_rxBuffer.clear();
        connect(m_client, SIGNAL(readyRead()), this, SLOT(onTcpReadyRead()));
        connect(m_client, SIGNAL(disconnected()), this, SLOT(onTcpDisconnected()));
        log(QString("<b>[TCP]</b> 下位机已连接: %1:%2")
                .arg(m_client->peerAddress().toString())
                .arg(m_client->peerPort()));
    }
}

void MainWindow::onTcpReadyRead()
{
    if (!m_client)
        return;

    m_rxBuffer += m_client->readAll();

    while (true) {
        int end = m_rxBuffer.indexOf('\n');
        int nul = m_rxBuffer.indexOf('\0');
        if (end < 0 || (nul >= 0 && nul < end))
            end = nul;
        if (end < 0)
            break;

        const QString msg = QString::fromUtf8(m_rxBuffer.left(end)).trimmed();
        m_rxBuffer.remove(0, end + 1);
        if (!msg.isEmpty())
            handleLowerMessage(msg);
    }

}

void MainWindow::onTcpDisconnected()
{
    if (!m_client)
        return;

    log("<font color='#ffb86c'><b>[TCP]</b> 下位机已断开</font>");
    m_client->deleteLater();
    m_client = 0;
    m_rxBuffer.clear();
}

void MainWindow::sendCommand(const QString &cmd)
{
    if (!m_client || m_client->state() != QAbstractSocket::ConnectedState) {
        log(QString("<font color='#ffb86c'><b>[TCP]</b> 下位机未连接, 未发送: %1</font>").arg(cmd));
        return;
    }

    m_client->write(cmd.toUtf8());
    m_client->write("\n");
    m_client->flush();
    log(QString("<b>[TCP → 下位机]</b> %1").arg(cmd));
}

void MainWindow::handleLowerMessage(const QString &msg)
{
    log(QString("<b>[下位机 → TCP]</b> %1").arg(htmlEscape(msg)));

    if (msg.startsWith("adc=", Qt::CaseInsensitive)) {
        QString value = msg.mid(4).trimmed();
        value.remove('%');
        bool ok = false;
        const double pctDouble = value.toDouble(&ok);
        if (!ok)
            return;

        const int pct = qBound(0, int(pctDouble + 0.5), 100);
        m_foodPercent = pct;
        {
            SignalBlocker b1(m_spinFoodLevel);
            SignalBlocker b2(m_sliderFoodLevel);
            m_spinFoodLevel->setValue(pct);
            m_sliderFoodLevel->setValue(pct);
        }

        const int raw = pct * qMax(1, m_cfg.adcMax) / 100;
        const double volt = 1.8 * raw / double(qMax(1, m_cfg.adcMax));
        if (m_adcTip)
            m_adcTip->setText(QString("下位机 ADC · %1V · 余量 %2%")
                                  .arg(volt, 0, 'f', 2).arg(pct));
        refreshFoodLevel();

        const bool want = m_auto && (pct <= 20);
        if (want != m_lightOn)
            applyLight(want, false);
        return;
    }

    if (msg.startsWith("temperature=", Qt::CaseInsensitive)) {
        bool ok = false;
        const double t = msg.mid(QString("temperature=").size()).trimmed().toDouble(&ok);
        if (ok)
            handleTemp(t, true);
        return;
    }

    if (msg.startsWith("key=", Qt::CaseInsensitive)) {
        const QString key = msg.mid(4).trimmed().toLower();
        if (key == "k1")
            applyLight(!m_lightOn, true);
        else if (key == "k2")
            onFeedClicked();
        else if (key == "k3")
            onPlayClicked();
    }
}

/* ============================================================
 * 控制逻辑
 * ============================================================ */

void MainWindow::syncFoodLevelUi(int percent)
{
    m_foodPercent = qBound(0, percent, 100);
    {
        SignalBlocker b1(m_spinFoodLevel);
        SignalBlocker b2(m_sliderFoodLevel);
        m_spinFoodLevel->setValue(m_foodPercent);
        m_sliderFoodLevel->setValue(m_foodPercent);
    }
    refreshFoodLevel();
}

void MainWindow::refreshFoodLevel()
{
    if (m_stateLabel) {
        m_stateLabel->setText(m_lightOn ? "● 提醒灯已点亮"
                                      : (m_foodPercent <= 20 ? "⚠ 余量不足" : "● 余量充足"));
        m_stateLabel->setProperty("on", m_lightOn ? "true" : "false");
        m_stateLabel->style()->unpolish(m_stateLabel);
        m_stateLabel->style()->polish(m_stateLabel);
    }

    /* ---- 余量显示(喂食版: 大百分比 + 两条进度条) ---- */
    if (m_adcValLabel)
        m_adcValLabel->setText(QString::number(m_foodPercent) + "%");
    if (m_adcBar)
        m_adcBar->setValue(m_foodPercent);
    if (m_adcBarSmall)
        m_adcBarSmall->setValue(m_foodPercent);
}

/* ---- 开关灯 ---- */
void MainWindow::onLightToggled(bool on)
{
    applyLight(on, true);
}

void MainWindow::applyLight(bool on, bool logIt)
{
    if (m_lightOn == on)
        return;

    m_lightOn = on;
    {
        SignalBlocker b(m_btnLight);
        m_btnLight->setChecked(on);
    }
    m_btnLight->setText(on ? "关闭提醒灯" : "打开提醒灯");
    refreshFoodLevel();
    sendCommand(on ? "LED_ON" : "LED_OFF");
    if (logIt)
        log(QString("<b>[LED]</b> 提醒灯%1").arg(on ? "开启" : "关闭"));
}

/* ---- 模拟食盆余量 ---- */
void MainWindow::onSpinChanged(int value)
{
    if (!m_cfg.simulation || (m_client && m_client->state() == QAbstractSocket::ConnectedState))
        return;
    {
        SignalBlocker b(m_sliderFoodLevel);
        m_sliderFoodLevel->setValue(value);
    }
    m_foodPercent = value;
    refreshFoodLevel();                 // 输入时先预览
}

void MainWindow::onSliderChanged(int value)
{
    if (!m_cfg.simulation || (m_client && m_client->state() == QAbstractSocket::ConnectedState))
        return;
    {
        SignalBlocker b(m_spinFoodLevel);
        m_spinFoodLevel->setValue(value);
    }
    m_foodPercent = value;
    refreshFoodLevel();
}

void MainWindow::onApplyFoodLevel()
{
    if (!m_cfg.simulation || (m_client && m_client->state() == QAbstractSocket::ConnectedState))
        return;
    syncFoodLevelUi(m_spinFoodLevel->value());
    pollAdc();
}

/* ---- 蜂鸣器播放 ---- */
void MainWindow::setTone(int freqHz)
{
    if (freqHz <= 0) {
        if (!m_cfg.simulation)
            sendCommand("BUZZER_OFF");
        return;
    }
    if (!m_cfg.simulation)
        sendCommand(QString("BUZZER_FREQ=%1").arg(freqHz));
}

void MainWindow::onPlayClicked()
{
    if (m_alarming) {
        log("<font color='#ffb86c'><b>[蜂鸣器]</b> 正在高温报警, 蜂鸣器由警报占用</font>");
        return;
    }
    if (!m_cfg.simulation) {
        sendCommand("play_music");
        log("<b>[音乐模块]</b> 已通知下位机播放《两只老虎》");
        return;
    }
    if (m_melodyTimer && m_melodyTimer->isActive())
        stopMelody();                 // 允许重新播放

    m_playing = true;
    m_melodyLoop = false;             // 门铃: 只播一遍
    m_melodyIndex = 0;
    m_beeped = false;
    QPushButton *b = m_btnMusic ? m_btnMusic : m_btnPlay;
    if (b) {
        b->setText("♪ 播放中…");
        if (b->isCheckable())
            b->setChecked(true);
    }
    log("<b>[音乐模块]</b> 试听喂食提示音乐(PC 模拟提示音)");

    onMelodyTick();                   // 立刻走第一步(模拟模式在此响一声)
    m_melodyTimer->start();
}

/* 手动喂食: 立即喂食按钮 / 空格键 */
void MainWindow::onFeedClicked()
{
    const QTime now = QTime::currentTime();
    m_feedToday++;
    m_lastFeed = now;
    if (m_feedCountL)
        m_feedCountL->setText(QString("今日喂食 %1 次 · 最近 %2")
                                  .arg(m_feedToday)
                                  .arg(now.toString("HH:mm:ss")));

    // 喂食 = 食盆加满(模拟电位器回到 100%); 真实板可在此触发喂食机构 GPIO
    syncFoodLevelUi(100);
    log(QString("<b>[手动喂食]</b> %1 触发, 食盆余量回满 100%")
            .arg(now.toString("HH:mm:ss")));

    // 喂食成功短提示音(音乐模块): 嘀嘀
    if (m_melodyTimer && m_melodyTimer->isActive())
        stopMelody();
    m_playing = true;
    m_melodyLoop = false;
    m_melodyIndex = 0;
    m_beeped = false;
    m_melody.clear();
    m_melody << 1046 << 1046 << 0 << 0 << 0;
    onMelodyTick();
    m_melodyTimer->start();
}

void MainWindow::onMelodyTick()
{
    if (m_melodyIndex >= m_melody.size()) {
        if (m_melodyLoop) {           // 警报音: 循环播放
            m_melodyIndex = 0;
            return;
        }
        m_melodyTimer->stop();
        setTone(0);                   // 停止发声
        m_playing = false;
        QPushButton *b = m_btnMusic ? m_btnMusic : m_btnPlay;
        if (b) {
            if (b->isCheckable())
                b->setChecked(false);
            b->setText(m_btnMusic ? "试听喂食提示音乐" : "♪  播放声音");
        }
        log("<b>[蜂鸣器]</b> 播放结束");
        return;
    }

    const int f = m_melody.at(m_melodyIndex++);

    if (m_cfg.simulation) {
        if (f > 0 && !m_beeped) {     // 模拟模式只响一次
            QApplication::beep();
            m_beeped = true;
        }
    } else {
        setTone(f);                   // 真实板: 用 PWM 发声
    }
}

/* ---- ADC ----
 * 喂食版界面: 余量 = 电位器滑条(模拟) 或 ADC 节点(真实), 并换算显示
 */
void MainWindow::pollAdc()
{
    if (m_client && m_client->state() == QAbstractSocket::ConnectedState)
        return;
    if (!m_cfg.simulation)
        return;

    const int pct = m_foodPercent;
    const int raw = pct * m_cfg.adcMax / 100;
    const double volt = 1.8 * raw / double(m_cfg.adcMax);
    if (m_adcTip)
        m_adcTip->setText(QString("原始 %1 · %2V · 余量 %3%")
                              .arg(raw).arg(volt, 0, 'f', 2).arg(pct));
    refreshFoodLevel();
    const bool want = m_auto && (pct <= 20);
    if (want != m_lightOn)
        applyLight(want, false);
}

/* ============================================================
 * DS18B20 温度 / 高温报警
 * ============================================================ */

void MainWindow::onTempPoll()
{
    if (!m_cfg.simulation || (m_client && m_client->state() == QAbstractSocket::ConnectedState))
        return; // 真实温度由下位机 TCP 上报。
    const qreal t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const double v = 24.0 + 9.0 * std::sin(t / 5.0) + 0.4 * std::sin(t / 0.9);
    handleTemp(v, true);
}

/* 收到新温度: 刷新界面并按阈值判断是否报警 */
void MainWindow::handleTemp(double t, bool valid)
{
    m_tempValid = valid;
    m_tempNow = t;
    if (m_tempVal)
        m_tempVal->setText(valid ? QString("%1 ℃").arg(t, 0, 'f', 1) : "---- ℃");

    // 报警中: 横幅里实时显示温度; 平时清空文字保持占位
    if (m_alarmBanner) {
        if (m_alarming) {
            const double th = m_spinTemp ? m_spinTemp->value() : m_cfg.tempThreshold;
            m_alarmBanner->setText(QString("⚠ 高温报警!  当前 %1 ℃ ≥ %2 ℃   ·   已点亮报警灯并播放警报")
                                       .arg(t, 0, 'f', 1).arg(th, 0, 'f', 1));
        } else if (!valid || !m_tempValid) {
            m_alarmBanner->setText("");
        }
    }
    evaluateAlarm();
}

/* 按 当前温度/阈值/回差 决定 报警 或 解除 */
void MainWindow::evaluateAlarm()
{
    if (!m_alarmEnabled || !m_tempValid)
        return;
    const double th   = m_spinTemp ? m_spinTemp->value() : m_cfg.tempThreshold;
    const double hyst = m_cfg.tempHysteresis;

    if (!m_alarming && m_tempNow >= th)
        startAlarm();
    else if (m_alarming && m_tempNow <= th - hyst)
        stopAlarm();
}

void MainWindow::startAlarm()
{
    if (m_alarming)
        return;
    m_alarming = true;
    m_blinkOn = false;
    log(QString("<font color='#ff6b6b'><b>[报警]</b> 温度 %1 ℃ 达到阈值, 触发高温报警!</font>")
            .arg(m_tempNow, 0, 'f', 1));

    if (m_alarmBanner) {
        m_alarmBanner->setProperty("flash", "false");
        m_alarmBanner->style()->unpolish(m_alarmBanner);
        m_alarmBanner->style()->polish(m_alarmBanner);
    }
    handleTemp(m_tempNow, true);          // 刷新横幅文字


    // 警报音乐(真实板用 PWM 循环播放; 模拟只提示一声)
    if (m_btnPlay && m_btnPlay->isCheckable())
        m_btnPlay->setEnabled(false);
    startAlarmMelody();

    // 界面报警横幅闪烁
    m_alarmBlink->start();
    onAlarmBlink();
}

void MainWindow::stopAlarm()
{
    if (!m_alarming)
        return;
    m_alarming = false;
    const double th = m_spinTemp ? m_spinTemp->value() : m_cfg.tempThreshold;
    log(QString("<b>[报警]</b> 温度 %1 ℃ 回落至 %2 ℃ 以下, 报警解除")
            .arg(m_tempNow, 0, 'f', 1).arg(th - m_cfg.tempHysteresis, 0, 'f', 1));

    if (m_alarmBanner) {
        m_alarmBanner->setText("");
        m_alarmBanner->setProperty("flash", "false");
        m_alarmBanner->style()->unpolish(m_alarmBanner);
        m_alarmBanner->style()->polish(m_alarmBanner);
    }
    m_alarmBlink->stop();
    stopMelody();
    if (m_btnPlay && m_btnPlay->isCheckable())
        m_btnPlay->setEnabled(true);

}

/* 循环警报音: “哔哔哔——哔” */
void MainWindow::startAlarmMelody()
{
    if (m_cfg.simulation) {
        QApplication::beep();             // PC 演示只响一声
        return;
    }
    if (m_melodyTimer && m_melodyTimer->isActive())
        stopMelody();
    m_melody.clear();
    m_melody << 880 << 880 << 880 << 0 << 880 << 880 << 880 << 0
             << 660 << 660 << 660 << 0 << 0 << 0;
    m_melodyIndex = 0;
    m_melodyLoop = true;                  // 循环直到解除报警
    m_playing = true;
    onMelodyTick();
    m_melodyTimer->start();
}

/* 停止旋律并静音蜂鸣器(门铃/警报通用) */
void MainWindow::stopMelody()
{
    if (m_melodyTimer && m_melodyTimer->isActive())
        m_melodyTimer->stop();
    m_melodyLoop = false;
    m_playing = false;
    m_melodyIndex = 0;
    setTone(0);
    QPushButton *b = m_btnMusic ? m_btnMusic : m_btnPlay;
    if (b) {
        if (b->isCheckable())
            b->setChecked(false);
        b->setText(m_btnMusic ? "试听喂食提示音乐" : "♪  播放声音");
    }
}

/* 报警灯 0.5s 交替闪烁 */
void MainWindow::onAlarmBlink()
{
    m_blinkOn = !m_blinkOn;
    if (m_alarmBanner) {
        m_alarmBanner->setProperty("flash", m_blinkOn ? "true" : "false");
        m_alarmBanner->style()->unpolish(m_alarmBanner);
        m_alarmBanner->style()->polish(m_alarmBanner);
    }

}

void MainWindow::onThresholdChanged(double)
{
    if (!m_spinTemp)
        return;
    log(QString("<b>[报警]</b> 阈值改为 %1 ℃").arg(m_spinTemp->value(), 0, 'f', 1));
    evaluateAlarm();
}

void MainWindow::onAlarmToggle(bool checked)
{
    if (!m_spinTemp)   // 喂食版界面没有温度卡片
        return;
    m_alarmEnabled = checked;
    if (!checked && m_alarming) {
        stopAlarm();
        log("<b>[报警]</b> 高温报警已被手动关闭");
    } else if (checked) {
        log(QString("<b>[报警]</b> 高温报警已开启, 阈值 %1 ℃(回差 %2 ℃)")
                .arg(m_cfg.tempThreshold, 0, 'f', 1)
                .arg(m_cfg.tempHysteresis, 0, 'f', 1));
        evaluateAlarm();
    }
}

void MainWindow::onAutoToggled(bool checked)
{
    m_auto = checked;
    log(checked ? "<b>[余量提醒]</b> 已开启: 余量 ≤20% 自动点亮提醒灯"
                : "<b>[余量提醒]</b> 已关闭");
    if (checked)
        applyLight(m_foodPercent <= 20, false);
    else if (m_lightOn)
        applyLight(false, false);
}

/* ---- 日志 ---- */
void MainWindow::log(const QString &s)
{
    if (!m_log)
        return;
    const QString time = QTime::currentTime().toString("HH:mm:ss");
    m_log->append(QString("<span style='color:#5c6c96'>[%1]</span> %2").arg(time, s));
}

/* ============================================================
 * QSS 美化
 * ============================================================ */
void MainWindow::applyStyle()
{
    QFile f(QDir::current().filePath("style.qss"));
    if (!f.exists())
        f.setFileName(QCoreApplication::applicationDirPath() + "/style.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(f.readAll()));
        f.close();
    }
}
