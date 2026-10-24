#include "mainwindow.h"

#include <QApplication>

// Usage: DiagProbe [description.uscxml] [database.ffxml]
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("DiagProbe");

    MainWindow window;
    window.show();
    window.openFiles(app.arguments().mid(1));

    return app.exec();
}
