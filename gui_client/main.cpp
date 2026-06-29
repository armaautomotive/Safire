#include "mainwindow.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Safire");
    QApplication::setOrganizationName("Safire");

    MainWindow window;
    if (QApplication::arguments().contains("--smoke-test")) {
        return 0;
    }

    window.show();

    return app.exec();
}
