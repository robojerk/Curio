#include "CurioSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>

CurioSettings &CurioSettings::instance()
{
    static CurioSettings settings;
    return settings;
}

QString CurioSettings::settingsPath() const
{
    if (!m_path.isEmpty())
        return m_path;
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty())
        return QString();
    return dataDir + QStringLiteral("/settings.json");
}

void CurioSettings::load()
{
    if (m_loaded)
        return;
    m_loaded = true;
    m_path = settingsPath();
    if (m_path.isEmpty())
        return;
    QFile file(m_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    m_githubPat = root.value(QStringLiteral("githubPat")).toString();
    m_gitlabPat = root.value(QStringLiteral("gitlabPat")).toString();
    m_codebergPat = root.value(QStringLiteral("codebergPat")).toString();
}

void CurioSettings::save() const
{
    const QString path = settingsPath();
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject root;
    if (!m_githubPat.isEmpty())
        root.insert(QStringLiteral("githubPat"), m_githubPat);
    if (!m_gitlabPat.isEmpty())
        root.insert(QStringLiteral("gitlabPat"), m_gitlabPat);
    if (!m_codebergPat.isEmpty())
        root.insert(QStringLiteral("codebergPat"), m_codebergPat);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
}

QString CurioSettings::githubPat() const
{
    const_cast<CurioSettings *>(this)->load();
    return m_githubPat;
}

QString CurioSettings::gitlabPat() const
{
    const_cast<CurioSettings *>(this)->load();
    return m_gitlabPat;
}

QString CurioSettings::codebergPat() const
{
    const_cast<CurioSettings *>(this)->load();
    return m_codebergPat;
}

QString CurioSettings::effectiveGithubPat() const
{
    const QString saved = githubPat();
    if (!saved.isEmpty())
        return saved;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString fromEnv = env.value(QStringLiteral("GITHUB_TOKEN")).trimmed();
    if (!fromEnv.isEmpty())
        return fromEnv;
    return env.value(QStringLiteral("GH_TOKEN")).trimmed();
}

QString CurioSettings::effectiveGitlabPat() const
{
    const QString saved = gitlabPat();
    if (!saved.isEmpty())
        return saved;
    return QProcessEnvironment::systemEnvironment()
            .value(QStringLiteral("GITLAB_TOKEN"))
            .trimmed();
}

QString CurioSettings::effectiveCodebergPat() const
{
    const QString saved = codebergPat();
    if (!saved.isEmpty())
        return saved;
    return QProcessEnvironment::systemEnvironment()
            .value(QStringLiteral("CODEBERG_TOKEN"))
            .trimmed();
}

void CurioSettings::setGithubPat(const QString &token)
{
    load();
    m_githubPat = token.trimmed();
    save();
}

void CurioSettings::setGitlabPat(const QString &token)
{
    load();
    m_gitlabPat = token.trimmed();
    save();
}

void CurioSettings::setCodebergPat(const QString &token)
{
    load();
    m_codebergPat = token.trimmed();
    save();
}
