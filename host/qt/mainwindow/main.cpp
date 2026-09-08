#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.resize(1280,760);
    w.setMinimumSize(1180,720);
    w.show();
    return a.exec();
}
