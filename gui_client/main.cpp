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
    //w.setFixedSize(x,y);
    //QRect rect(0, 0, 240, 320);
    //frame->setFrameShape(QFrame::Box);
    //frame->setLineWidth(3);
    //frame->setFrameShadow(QFrame::Plain);
    //frame->setGeometry(rect);
 
    //adjustSize();

    //void createHorizontalGroupBox();

    //QPalette pal = palette();
    //pal.setColor(QPalette::Background, Qt::black);
    //w.setAutoFillBackground(true);
    //w.setPalette(pal);

    // QPushButton hello( "Hello world!", 0 );
    //    hello.resize( 100, 30 );
    //    a.setMainWidget( &hello );
    //    hello.show();

    w.show();

    return a.exec();
}
