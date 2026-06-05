#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

#include "backend/SandboxedFlatpakEnv.h"
#include "models/AppInfo.h"

namespace {

QString catalogPageUrl(const QString &repoId, const QString &appId)
{
    if (repoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("https://flathub.org/apps/%1").arg(appId);
    return QString();
}

QVector<AppInfo> loadCatalogViaFlatpakCli(const QString &repoId)
{
    QProcess proc;
    proc.start(SandboxedFlatpakEnv::flatpakExecutable(),
               {QStringLiteral("remote-ls"),
                repoId,
                QStringLiteral("--app"),
                QStringLiteral("--columns=application,version,name")});
    if (!proc.waitForFinished(900'000) || proc.exitStatus() != QProcess::NormalExit
        || proc.exitCode() != 0) {
        return {};
    }

    QVector<AppInfo> apps;
    const QList<QByteArray> lines = proc.readAllStandardOutput().split('\n');
    for (const QByteArray &lineBytes : lines) {
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("Application")))
            continue;
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;
        AppInfo app;
        app.id = parts.at(0);
        if (app.id.isEmpty() || !app.id.contains(QLatin1Char('.')))
            continue;
        if (parts.size() > 1)
            app.version = parts.at(1);
        if (parts.size() > 2)
            app.name = parts.mid(2).join(QLatin1Char(' '));
        if (app.name.isEmpty())
            app.name = app.id;
        app.repoId = repoId;
        app.storePageUrl = catalogPageUrl(repoId, app.id);
        apps.append(app);
    }
    return apps;
}

} // namespace

int main(int argc, char *argv[])
{
    SandboxedFlatpakEnv::configure();
    QCoreApplication app(argc, argv);
    const QString repoId = argc > 1 ? QString::fromLocal8Bit(argv[1]).trimmed()
                                    : QStringLiteral("flathub");

    const QVector<AppInfo> apps = loadCatalogViaFlatpakCli(repoId);
    QTextStream out(stdout);
    for (const AppInfo &info : apps) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), info.id);
        obj.insert(QStringLiteral("name"), info.name);
        obj.insert(QStringLiteral("summary"), info.summary);
        obj.insert(QStringLiteral("version"), info.version);
        obj.insert(QStringLiteral("repoId"), info.repoId);
        obj.insert(QStringLiteral("storePageUrl"), info.storePageUrl);
        out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    }
    return apps.isEmpty() ? 1 : 0;
}
