QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# Qt 4.8 的 qmake 在部分旧环境中不会自动加 C++11 参数, 这里给 g++ 做兜底。
unix: QMAKE_CXXFLAGS += -std=c++11
win32-g++*: QMAKE_CXXFLAGS += -std=c++11

TARGET = ledcontrol
TEMPLATE = app

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    hardware.cpp

HEADERS += \
    feedschedule.h \
    mainwindow.h \
    hardware.h

# 界面全部在代码中搭建(QSS 内嵌), 不再使用 .ui 文件

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
