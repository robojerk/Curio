#include <QApplication>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Curio");
    app.setApplicationDisplayName("Curio");
    app.setOrganizationName("Curio");
    app.setDesktopFileName("curio.desktop");
    app.setWindowIcon(QIcon(":/icons/curio"));

    MainWindow w;
    w.show();

    return app.exec();
}

