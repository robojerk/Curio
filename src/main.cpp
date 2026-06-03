#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>
#include <QTimer>

#include "ui/MainWindow.h"

namespace {

QString installedDesktopFileName()
{
    const QString desktopId = QStringLiteral("io.github.curio.Curio.desktop");
    const QStringList appDirs =
            QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dir : appDirs) {
        if (QFileInfo::exists(dir + QLatin1Char('/') + desktopId))
            return desktopId;
    }
    return {};
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Curio");
    app.setApplicationDisplayName("Curio");
    app.setOrganizationName("Curio");
    const QString desktopFile = installedDesktopFileName();
    if (!desktopFile.isEmpty())
        app.setDesktopFileName(desktopFile);
    app.setWindowIcon(QIcon(":/icons/curio"));

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.size() > 1) {
        const QStringList fileArgs = args.mid(1);
        QTimer::singleShot(0, &w, [fileArgs, &w]() {
            for (const QString &arg : fileArgs)
                w.handleOpenFileRequest(arg);
        });
    }

    return app.exec();
}

