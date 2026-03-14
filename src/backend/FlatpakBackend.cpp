#include "FlatpakBackend.h"
#include "AppStreamProvider.h"

#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QUrl>
#include <limits>
#include <QSet>
#include <algorithm>

namespace {
void dedupeScreenshotUrls(AppInfo &app)
{
    if (app.screenshotUrls.isEmpty())
        return;
    QSet<QString> seen;
    QStringList unique;
    for (const QString &raw : app.screenshotUrls) {
        const QUrl url(raw);
        const QString key = url.isValid()
                ? url.toString(QUrl::RemoveQuery | QUrl::RemoveFragment)
                : raw;
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        unique.append(raw);
    }
    app.screenshotUrls = unique;
}

QVector<AppInfo> pickTopByScore(const QVector<AppInfo> &apps,
                                const std::function<int (const AppInfo &, int)> &scoreFn,
                                int limit)
{
    struct Ranked {
        AppInfo app;
        int score = 0;
        int index = 0;
    };

    QVector<Ranked> ranked;
    ranked.reserve(apps.size());
    for (int i = 0; i < apps.size(); ++i) {
        ranked.push_back({apps.at(i), scoreFn(apps.at(i), i), i});
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.index < b.index;
    });

    QVector<AppInfo> selected;
    selected.reserve(limit);
    QSet<QString> seenIds;
    for (const Ranked &r : ranked) {
        if (selected.size() >= limit)
            break;
        if (r.app.id.isEmpty() || seenIds.contains(r.app.id))
            continue;
        seenIds.insert(r.app.id);
        selected.push_back(r.app);
    }
    return selected;
}
}

FlatpakBackend::FlatpakBackend(QObject *parent)
    : QObject(parent)
    , m_appStreamProvider(new AppStreamProvider(this))
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty()) {
        QDir().mkpath(dataDir);
        m_cachePath = dataDir + QStringLiteral("/search_cache.json");
        loadSearchCache();
    }
    if (qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG")) {
        qDebug() << "FlatpakBackend: AppStream provider available =" << m_appStreamProvider->isAvailable();
    }
}

void FlatpakBackend::refreshInstalled()
{
    QStringList args{QStringLiteral("list"),
                     QStringLiteral("--app"),
                     QStringLiteral("--columns=application,name,description,version")};

    runFlatpakCommand(args,
                      [this](const QByteArray &out) {
                          auto apps = parseListOutput(out);
                          for (auto &app : apps) {
                              app.installed = true;
                              m_appStreamProvider->enrich(app);
                          }
                          emit installedAppsUpdated(apps);
                      });
}

void FlatpakBackend::search(const QString &query)
{
    QVector<AppInfo> cached;
    if (tryGetCachedSearch(query, &cached)) {
        // Refresh enriched metadata even on cache hit so icon/screenshot fixes apply immediately.
        for (auto &app : cached) {
            app.screenshotUrls.clear();
            app.iconUrl.clear();
            m_appStreamProvider->enrich(app);
            dedupeScreenshotUrls(app);
        }
        emit searchResultsUpdated(cached);
        if (qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG")) {
            qDebug() << "FlatpakBackend: cache hit for query" << query;
        }
        return;
    }

    QStringList args{QStringLiteral("search"),
                     QStringLiteral("--columns=application,name,description,version"),
                     query};

    runFlatpakCommand(args,
                      [this, query](const QByteArray &out) {
                          auto apps = parseListOutput(out);
                          for (auto &app : apps) {
                              m_appStreamProvider->enrich(app);
                              dedupeScreenshotUrls(app);
                          }
                          putCachedSearch(query, apps);
                          emit searchResultsUpdated(apps);
                      });
}

void FlatpakBackend::fetchFlathubSuggestions()
{
    QStringList args{
        QStringLiteral("remote-ls"),
        QStringLiteral("flathub"),
        QStringLiteral("--app"),
        QStringLiteral("--columns=application,name,description,version")
    };

    runFlatpakCommand(args,
                      [this](const QByteArray &out) {
                          QVector<AppInfo> apps = parseListOutput(out);
                          if (apps.isEmpty()) {
                              emit flathubSuggestionsUpdated({});
                              emit flathubCollectionsUpdated({}, {}, {}, {});
                              return;
                          }

                          const QStringList keywords = {
                              QStringLiteral("browser"), QStringLiteral("chat"),
                              QStringLiteral("music"), QStringLiteral("video"),
                              QStringLiteral("photo"), QStringLiteral("graphics"),
                              QStringLiteral("office"), QStringLiteral("game"),
                              QStringLiteral("dev"), QStringLiteral("code"),
                              QStringLiteral("education"), QStringLiteral("network"),
                              QStringLiteral("science"), QStringLiteral("system"),
                              QStringLiteral("utility"), QStringLiteral("mobile")
                          };

                          const QSet<QString> popularIds = {
                              QStringLiteral("org.mozilla.firefox"),
                              QStringLiteral("org.libreoffice.LibreOffice"),
                              QStringLiteral("org.videolan.VLC"),
                              QStringLiteral("org.gimp.GIMP"),
                              QStringLiteral("org.inkscape.Inkscape"),
                              QStringLiteral("com.valvesoftware.Steam"),
                              QStringLiteral("com.spotify.Client"),
                              QStringLiteral("com.discordapp.Discord"),
                              QStringLiteral("org.signal.Signal"),
                              QStringLiteral("com.slack.Slack"),
                              QStringLiteral("org.kde.kdenlive"),
                              QStringLiteral("md.obsidian.Obsidian")
                          };

                          QVector<AppInfo> trending = pickTopByScore(apps, [keywords](const AppInfo &app, int) {
                              const QString haystack = (app.name + QLatin1Char(' ') + app.summary).toLower();
                              int score = 0;
                              for (const QString &kw : keywords) {
                                  if (haystack.contains(kw))
                                      ++score;
                              }
                              if (!app.summary.trimmed().isEmpty())
                                  score += 2;
                              return score;
                          }, 24);

                          QVector<AppInfo> popular = pickTopByScore(apps, [popularIds, keywords](const AppInfo &app, int) {
                              const QString haystack = (app.name + QLatin1Char(' ') + app.summary).toLower();
                              int score = 0;
                              if (popularIds.contains(app.id))
                                  score += 50;
                              if (haystack.contains(QStringLiteral("official")))
                                  score += 3;
                              for (const QString &kw : keywords) {
                                  if (haystack.contains(kw))
                                      ++score;
                              }
                              if (!app.summary.trimmed().isEmpty())
                                  score += 2;
                              return score;
                          }, 24);

                          QVector<AppInfo> recent = pickTopByScore(apps, [](const AppInfo &app, int idx) {
                              // Deterministic "fresh picks" spread when true recent data is unavailable.
                              return static_cast<int>(qHash(app.id) % 10000) - (idx / 10);
                          }, 24);

                          QVector<AppInfo> updated = pickTopByScore(apps, [](const AppInfo &app, int) {
                              int score = 0;
                              const QString v = app.version;
                              QRegularExpression rx(QStringLiteral("(\\d+)"));
                              QRegularExpressionMatchIterator it = rx.globalMatch(v);
                              int factor = 10000;
                              while (it.hasNext() && factor > 1) {
                                  const auto m = it.next();
                                  score += m.captured(1).toInt() * factor;
                                  factor /= 10;
                              }
                              if (!v.trimmed().isEmpty())
                                  score += 5;
                              return score;
                          }, 24);

                          auto enrichList = [this](QVector<AppInfo> &list) {
                              for (auto &app : list) {
                                  m_appStreamProvider->enrich(app);
                                  dedupeScreenshotUrls(app);
                              }
                          };
                          enrichList(trending);
                          enrichList(popular);
                          enrichList(recent);
                          enrichList(updated);

                          emit flathubSuggestionsUpdated(trending);
                          emit flathubCollectionsUpdated(trending, popular, recent, updated);
                      });
}

void FlatpakBackend::listRemotes()
{
    QStringList args{
        QStringLiteral("remotes"),
        QStringLiteral("--columns=name,url")
    };

    runFlatpakCommand(args,
                      [this](const QByteArray &out) {
                          QVector<QPair<QString, QString>> remotes;
                          QTextStream stream(out);
                          while (!stream.atEnd()) {
                              const QString line = stream.readLine().trimmed();
                              if (line.isEmpty())
                                  continue;
                              const QStringList parts = line.split(QLatin1Char('\t'));
                              if (parts.isEmpty())
                                  continue;
                              const QString name = parts.value(0).trimmed();
                              if (name.isEmpty() || name.compare(QStringLiteral("Name"), Qt::CaseInsensitive) == 0)
                                  continue;
                              const QString url = parts.value(1).trimmed();
                              remotes.append(qMakePair(name, url));
                          }
                          emit remotesUpdated(remotes);
                      },
                      [this](int exitCode, const QByteArray &stderrData) {
                          if (exitCode != 0) {
                              const QString msg = QString::fromUtf8(stderrData.trimmed());
                              emit remoteAddFinished(false, msg.isEmpty() ? tr("Failed to list remotes") : msg);
                          }
                      });
}

void FlatpakBackend::addRemote(const QString &name, const QString &url)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedUrl = url.trimmed();
    if (trimmedName.isEmpty() || trimmedUrl.isEmpty()) {
        emit remoteAddFinished(false, tr("Remote name and URL are required"));
        return;
    }

    QStringList args{
        QStringLiteral("remote-add"),
        QStringLiteral("--if-not-exists"),
        QStringLiteral("--user"),
        trimmedName,
        trimmedUrl
    };

    runFlatpakCommand(args,
                      nullptr,
                      [this](int exitCode, const QByteArray &stderrData) {
                          if (exitCode == 0) {
                              emit remoteAddFinished(true, tr("Remote added"));
                              listRemotes();
                              return;
                          }
                          const QString msg = QString::fromUtf8(stderrData.trimmed());
                          emit remoteAddFinished(false, msg.isEmpty() ? tr("Failed to add remote") : msg);
                      });
}

void FlatpakBackend::removeRemote(const QString &name)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        emit remoteRemoveFinished(false, tr("Select a remote to remove"));
        return;
    }
    if (trimmedName.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        emit remoteRemoveFinished(false, tr("Flathub cannot be removed"));
        return;
    }

    QStringList args{
        QStringLiteral("remote-delete"),
        QStringLiteral("--user"),
        trimmedName
    };

    runFlatpakCommand(args,
                      nullptr,
                      [this](int exitCode, const QByteArray &stderrData) {
                          if (exitCode == 0) {
                              emit remoteRemoveFinished(true, tr("Remote removed"));
                              listRemotes();
                              return;
                          }
                          const QString msg = QString::fromUtf8(stderrData.trimmed());
                          emit remoteRemoveFinished(false, msg.isEmpty() ? tr("Failed to remove remote") : msg);
                      });
}

void FlatpakBackend::install(const QString &appId)
{
    Operation op;
    op.appId = appId;
    op.type = OperationType::Install;
    op.status = OperationStatus::Running;
    emit operationStarted(op);

    QStringList args{QStringLiteral("install"),
                     QStringLiteral("-y"),
                     QStringLiteral("--noninteractive"),
                     appId};

    runFlatpakCommand(args,
                      [this, op](const QByteArray &) mutable {
                          emit operationProgress(op);
                      },
                      [this, op, appId](int exitCode, const QByteArray &stderrData) mutable {
                          if (exitCode == 0) {
                              op.status = OperationStatus::Succeeded;
                              op.message = tr("Installed successfully");
                              emit operationFinished(op);
                              return;
                          }

                          // Retry by explicitly targeting Flathub for discovery-fed apps.
                          QStringList retryArgs{QStringLiteral("install"),
                                                QStringLiteral("-y"),
                                                QStringLiteral("--noninteractive"),
                                                QStringLiteral("flathub"),
                                                appId};
                          runFlatpakCommand(retryArgs,
                                            [this, op](const QByteArray &) mutable {
                                                emit operationProgress(op);
                                            },
                                            [this, op, stderrData](int retryExitCode, const QByteArray &retryStderr) mutable {
                                                op.status = retryExitCode == 0 ? OperationStatus::Succeeded
                                                                               : OperationStatus::Failed;
                                                if (op.status == OperationStatus::Succeeded) {
                                                    op.message = tr("Installed successfully (flathub)");
                                                } else {
                                                    const QByteArray err = retryStderr.trimmed().isEmpty()
                                                            ? stderrData.trimmed()
                                                            : retryStderr.trimmed();
                                                    op.message = err.isEmpty()
                                                            ? tr("Install failed")
                                                            : QString::fromUtf8(err);
                                                }
                                                emit operationFinished(op);
                                            });
                      });
}

void FlatpakBackend::uninstall(const QString &appId)
{
    Operation op;
    op.appId = appId;
    op.type = OperationType::Uninstall;
    op.status = OperationStatus::Running;
    emit operationStarted(op);

    QStringList args{QStringLiteral("uninstall"),
                     QStringLiteral("-y"),
                     appId};

    runFlatpakCommand(args,
                      [this, op](const QByteArray &) mutable {
                          emit operationProgress(op);
                      },
                      [this, op](int exitCode, const QByteArray &stderrData) mutable {
                          op.status = exitCode == 0 ? OperationStatus::Succeeded
                                                    : OperationStatus::Failed;
                          if (op.status == OperationStatus::Succeeded) {
                              op.message = tr("Uninstalled successfully");
                          } else {
                              const QByteArray err = stderrData.trimmed();
                              op.message = err.isEmpty()
                                      ? tr("Uninstall failed")
                                      : QString::fromUtf8(err);
                          }
                          emit operationFinished(op);
                      });
}

void FlatpakBackend::update(const QString &appId)
{
    Operation op;
    op.appId = appId;
    op.type = OperationType::Update;
    op.status = OperationStatus::Running;
    emit operationStarted(op);

    QStringList args{QStringLiteral("update"),
                     QStringLiteral("-y"),
                     appId};

    runFlatpakCommand(args,
                      [this, op](const QByteArray &) mutable {
                          emit operationProgress(op);
                      },
                      [this, op](int exitCode, const QByteArray &stderrData) mutable {
                          op.status = exitCode == 0 ? OperationStatus::Succeeded
                                                    : OperationStatus::Failed;
                          if (op.status == OperationStatus::Succeeded) {
                              op.message = tr("Updated successfully");
                          } else {
                              const QByteArray err = stderrData.trimmed();
                              op.message = err.isEmpty()
                                      ? tr("Update failed")
                                      : QString::fromUtf8(err);
                          }
                          emit operationFinished(op);
                      });
}

QVector<AppInfo> FlatpakBackend::parseListOutput(const QByteArray &output) const
{
    QVector<AppInfo> result;
    QTextStream stream(output);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // flatpak --columns output is typically tab-separated, but depending on
        // environment it may appear as generic whitespace. Be robust here.
        QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 3) {
            parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                               Qt::SkipEmptyParts);
        }

        if (parts.size() < 3)
            continue;

        AppInfo info;
        info.id = parts.value(0);
        // Ignore CLI status lines like "No matches found".
        if (!info.id.contains(QLatin1Char('.')))
            continue;
        info.name = parts.value(1);
        if (parts.size() >= 4) {
            info.version = parts.last();
            // Everything between name and version is treated as summary.
            const int summaryStart = 2;
            const int summaryCount = parts.size() - 3;
            info.summary = summaryCount > 0
                    ? parts.mid(summaryStart, summaryCount).join(QLatin1Char(' '))
                    : QString();
        } else {
            info.summary = parts.value(2);
        }
        // Most Flatpak apps install an icon matching the app-id in the icon theme.
        info.iconName = info.id;
        result.append(info);
    }
    return result;
}

void FlatpakBackend::runFlatpakCommand(const QStringList &arguments,
                                       std::function<void (const QByteArray &)> onStdout,
                                       std::function<void (int, const QByteArray &)> onFinished)
{
    auto *process = new QProcess(this);
    QString program = QStringLiteral("flatpak");
    auto *stdoutBuffer = new QByteArray();
    auto *stderrBuffer = new QByteArray();

    if (qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG")) {
        qDebug() << "FlatpakBackend: running flatpak" << arguments;
    }

    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [process, stdoutBuffer]() {
        stdoutBuffer->append(process->readAllStandardOutput());
    });
    QObject::connect(process, &QProcess::readyReadStandardError, this, [process, stderrBuffer]() {
        stderrBuffer->append(process->readAllStandardError());
    });
    QObject::connect(process, &QProcess::errorOccurred, this, [process](QProcess::ProcessError e) {
        qWarning() << "FlatpakBackend: process error" << e << "for" << process->program() << process->arguments();
    });

    QObject::connect(process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     this,
                     [process, onStdout, onFinished, stdoutBuffer, stderrBuffer](int exitCode, QProcess::ExitStatus) {
                         if (onStdout) {
                             onStdout(*stdoutBuffer);
                         }
                         if (exitCode != 0) {
                             qWarning() << "FlatpakBackend: command failed:"
                                        << process->program() << process->arguments()
                                        << "stderr:" << QString::fromUtf8(stderrBuffer->trimmed());
                         }
                         if (onFinished) {
                             onFinished(exitCode, *stderrBuffer);
                         }
                         delete stdoutBuffer;
                         delete stderrBuffer;
                         process->deleteLater();
                     });

    process->start(program, arguments);
}

QString FlatpakBackend::cacheKeyForQuery(const QString &query) const
{
    return query.trimmed().toLower();
}

bool FlatpakBackend::tryGetCachedSearch(const QString &query, QVector<AppInfo> *apps) const
{
    if (!apps)
        return false;
    const QString key = cacheKeyForQuery(query);
    if (key.isEmpty())
        return false;
    if (!m_searchCache.contains(key) || !m_searchCacheTimestamps.contains(key))
        return false;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ts = m_searchCacheTimestamps.value(key);
    if (now - ts > m_cacheTtlMs)
        return false;

    *apps = m_searchCache.value(key);
    return true;
}

void FlatpakBackend::putCachedSearch(const QString &query, const QVector<AppInfo> &apps)
{
    const QString key = cacheKeyForQuery(query);
    if (key.isEmpty())
        return;
    m_searchCache.insert(key, apps);
    m_searchCacheTimestamps.insert(key, QDateTime::currentMSecsSinceEpoch());

    // Keep cache bounded by removing oldest entries.
    while (m_searchCache.size() > m_cacheMaxEntries) {
        QString oldestKey;
        qint64 oldestTs = std::numeric_limits<qint64>::max();
        for (auto it = m_searchCacheTimestamps.cbegin(); it != m_searchCacheTimestamps.cend(); ++it) {
            if (it.value() < oldestTs) {
                oldestTs = it.value();
                oldestKey = it.key();
            }
        }
        if (oldestKey.isEmpty())
            break;
        m_searchCache.remove(oldestKey);
        m_searchCacheTimestamps.remove(oldestKey);
    }
    saveSearchCache();
}

void FlatpakBackend::loadSearchCache()
{
    if (m_cachePath.isEmpty())
        return;

    QFile f(m_cachePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(1) < m_cacheSchemaVersion)
        return;

    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QJsonValue &entryVal : entries) {
        const QJsonObject entryObj = entryVal.toObject();
        const QString key = entryObj.value(QStringLiteral("query")).toString();
        const qint64 ts = static_cast<qint64>(entryObj.value(QStringLiteral("ts")).toDouble());
        if (key.isEmpty() || ts <= 0 || (now - ts > m_cacheTtlMs))
            continue;

        QVector<AppInfo> apps;
        const QJsonArray appsArray = entryObj.value(QStringLiteral("apps")).toArray();
        for (const QJsonValue &appVal : appsArray) {
            const QJsonObject appObj = appVal.toObject();
            AppInfo app;
            app.id = appObj.value(QStringLiteral("id")).toString();
            app.name = appObj.value(QStringLiteral("name")).toString();
            app.summary = appObj.value(QStringLiteral("summary")).toString();
            app.version = appObj.value(QStringLiteral("version")).toString();
            app.iconName = appObj.value(QStringLiteral("iconName")).toString();
            app.installed = appObj.value(QStringLiteral("installed")).toBool();
            app.longDescription = appObj.value(QStringLiteral("longDescription")).toString();
            app.iconUrl = appObj.value(QStringLiteral("iconUrl")).toString();
            for (const QJsonValue &u : appObj.value(QStringLiteral("screenshotUrls")).toArray())
                app.screenshotUrls.append(u.toString());
            app.developerName = appObj.value(QStringLiteral("developerName")).toString();
            app.projectLicense = appObj.value(QStringLiteral("projectLicense")).toString();
            app.homepageUrl = appObj.value(QStringLiteral("homepageUrl")).toString();
            app.bugtrackerUrl = appObj.value(QStringLiteral("bugtrackerUrl")).toString();
            app.donateUrl = appObj.value(QStringLiteral("donateUrl")).toString();
            app.helpUrl = appObj.value(QStringLiteral("helpUrl")).toString();
            app.translateUrl = appObj.value(QStringLiteral("translateUrl")).toString();
            for (const QJsonValue &c : appObj.value(QStringLiteral("categories")).toArray())
                app.categories.append(c.toString());
            dedupeScreenshotUrls(app);
            apps.append(app);
        }

        m_searchCache.insert(key, apps);
        m_searchCacheTimestamps.insert(key, ts);
    }
}

void FlatpakBackend::saveSearchCache() const
{
    if (m_cachePath.isEmpty())
        return;

    QJsonArray entries;
    for (auto it = m_searchCache.cbegin(); it != m_searchCache.cend(); ++it) {
        const QString &key = it.key();
        const QVector<AppInfo> &apps = it.value();

        QJsonArray appsArray;
        for (const AppInfo &app : apps) {
            QJsonObject appObj;
            appObj.insert(QStringLiteral("id"), app.id);
            appObj.insert(QStringLiteral("name"), app.name);
            appObj.insert(QStringLiteral("summary"), app.summary);
            appObj.insert(QStringLiteral("version"), app.version);
            appObj.insert(QStringLiteral("iconName"), app.iconName);
            appObj.insert(QStringLiteral("installed"), app.installed);
            appObj.insert(QStringLiteral("longDescription"), app.longDescription);
            appObj.insert(QStringLiteral("iconUrl"), app.iconUrl);

            QJsonArray screenshots;
            for (const QString &u : app.screenshotUrls)
                screenshots.append(u);
            appObj.insert(QStringLiteral("screenshotUrls"), screenshots);

            appObj.insert(QStringLiteral("developerName"), app.developerName);
            appObj.insert(QStringLiteral("projectLicense"), app.projectLicense);
            appObj.insert(QStringLiteral("homepageUrl"), app.homepageUrl);
            appObj.insert(QStringLiteral("bugtrackerUrl"), app.bugtrackerUrl);
            appObj.insert(QStringLiteral("donateUrl"), app.donateUrl);
            appObj.insert(QStringLiteral("helpUrl"), app.helpUrl);
            appObj.insert(QStringLiteral("translateUrl"), app.translateUrl);

            QJsonArray categories;
            for (const QString &cat : app.categories)
                categories.append(cat);
            appObj.insert(QStringLiteral("categories"), categories);

            appsArray.append(appObj);
        }

        QJsonObject entry;
        entry.insert(QStringLiteral("query"), key);
        entry.insert(QStringLiteral("ts"), static_cast<double>(m_searchCacheTimestamps.value(key)));
        entry.insert(QStringLiteral("apps"), appsArray);
        entries.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), m_cacheSchemaVersion);
    root.insert(QStringLiteral("entries"), entries);
    QFile f(m_cachePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.close();
}

