#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("UDScribe");

    MainWindow window;
    window.show();

    return app.exec();
}
