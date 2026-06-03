#include "FlatpakRemoteCatalog.h"

#include <QHash>

namespace {

struct CatalogEntry {
    QString displayName;
    QString storePagePattern; // empty = no per-app catalog page; use %1 for app id
};

QString normalizedOrigin(const QString &origin)
{
    return origin.trimmed().toLower();
}

const QHash<QString, CatalogEntry> &catalogTable()
{
    static const QHash<QString, CatalogEntry> table = []() {
        QHash<QString, CatalogEntry> entries;
        const auto add = [&entries](const QString &key, const QString &name, const QString &pattern = {}) {
            entries.insert(key, CatalogEntry{name, pattern});
        };
        add(QStringLiteral("flathub"),
            QStringLiteral("Flathub"),
            QStringLiteral("https://flathub.org/apps/%1"));
        add(QStringLiteral("flathub-beta"),
            QStringLiteral("Flathub Beta"),
            QStringLiteral("https://flathub.org/apps/%1"));
        add(QStringLiteral("flathub-verified"),
            QStringLiteral("Flathub Verified"),
            QStringLiteral("https://flathub.org/apps/%1"));
        add(QStringLiteral("flathub-floss"),
            QStringLiteral("Flathub FLOSS"),
            QStringLiteral("https://flathub.org/apps/%1"));
        add(QStringLiteral("flathub-verified_floss"),
            QStringLiteral("Flathub Verified FLOSS"),
            QStringLiteral("https://flathub.org/apps/%1"));
        add(QStringLiteral("fedora"), QStringLiteral("Fedora"));
        add(QStringLiteral("elementaryos"), QStringLiteral("Elementary OS"));
        add(QStringLiteral("gnome-nightly"), QStringLiteral("GNOME Nightly"));
        add(QStringLiteral("igalia"), QStringLiteral("Igalia"));
        add(QStringLiteral("pureos"), QStringLiteral("PureOS"));
        return entries;
    }();
    return table;
}

QString humanizeOrigin(const QString &origin)
{
    QString label = origin.trimmed();
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    const QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList titled;
    titled.reserve(words.size());
    for (QString word : words) {
        if (word.isEmpty())
            continue;
        word[0] = word.at(0).toUpper();
        titled.append(word);
    }
    return titled.join(QLatin1Char(' '));
}

} // namespace

namespace FlatpakRemoteCatalog {

QString displayNameForOrigin(const QString &origin)
{
    const QString key = normalizedOrigin(origin);
    if (key.isEmpty())
        return QString();
    const auto it = catalogTable().constFind(key);
    if (it != catalogTable().cend())
        return it.value().displayName;
    return humanizeOrigin(origin);
}

QString catalogPageUrlForApp(const QString &origin, const QString &appId)
{
    if (appId.isEmpty())
        return QString();
    const QString key = normalizedOrigin(origin);
    const auto it = catalogTable().constFind(key);
    if (it == catalogTable().cend() || it.value().storePagePattern.isEmpty())
        return QString();
    return it.value().storePagePattern.arg(appId);
}

QString catalogPageLabelForOrigin(const QString &origin)
{
    const QString name = displayNameForOrigin(origin);
    if (name.isEmpty())
        return QString();
    return QStringLiteral("%1 Page").arg(name);
}

bool isCatalogOrigin(const QString &origin)
{
    const QString key = normalizedOrigin(origin);
    if (key.isEmpty())
        return false;
    return catalogTable().contains(key);
}

bool isBundleInstallOrigin(const QString &origin)
{
    const QString key = normalizedOrigin(origin);
    return key.endsWith(QStringLiteral("-origin"));
}

} // namespace FlatpakRemoteCatalog
