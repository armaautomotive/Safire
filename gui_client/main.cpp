#include "mainwindow.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    QDesktopWidget dw;
    int x=dw.width()*0.7;
    int y=dw.height()*0.7;
    w.setFixedSize(x,y);

    //QPalette pal = palette();
    //pal.setColor(QPalette::Background, Qt::black);
    //w.setAutoFillBackground(true);
    //w.setPalette(pal);


    w.show();

    return a.exec();
}
