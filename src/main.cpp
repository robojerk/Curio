#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>
#include <QTimer>

#include "ui/MainWindow.h"

namespace {

void configureFlatpakIconThemePaths()
{
    QStringList paths = QIcon::themeSearchPaths();
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QStringList flatpakIconRoots = {
        home + QStringLiteral("/.local/share/flatpak/exports/share/icons"),
        QStringLiteral("/var/lib/flatpak/exports/share/icons"),
    };
    for (const QString &root : flatpakIconRoots) {
        if (QDir(root).exists() && !paths.contains(root))
            paths.prepend(root);
    }
    QIcon::setThemeSearchPaths(paths);
}

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
    configureFlatpakIconThemePaths();
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

