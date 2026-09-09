#include "mainwindow.h"

#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QTextCodec>
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    // Source string literals are UTF-8; Qt 4 defaults to Latin-1.
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForCStrings(utf8);
    QTextCodec::setCodecForTr(utf8);
#endif
    MainWindow w;
    w.show();
    return a.exec();
}
