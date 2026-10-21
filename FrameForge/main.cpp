#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("FrameForge");

    MainWindow window;
    window.show();

    return app.exec();
}
