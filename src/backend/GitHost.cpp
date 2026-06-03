#include "GitHost.h"

#include <QRegularExpression>
#include <QUrl>

namespace {
QString stripRepoSuffix(QString slug)
{
    slug = slug.trimmed();
    while (slug.endsWith(QLatin1Char('/')))
        slug.chop(1);
    if (slug.endsWith(QStringLiteral(".git"), Qt::CaseInsensitive))
        slug.chop(4);
    return slug;
}

std::optional<ParsedGitRepo> parseHttpsRepo(const QUrl &url)
{
    static const QRegularExpression repoPathRx(
        QStringLiteral("^/([^/]+)/([^/]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = repoPathRx.match(url.path());
    if (!match.hasMatch())
        return std::nullopt;

    const QString host = url.host().toLower();
    GitHostKind kind;
    if (host == QStringLiteral("github.com"))
        kind = GitHostKind::GitHub;
    else if (host == QStringLiteral("gitlab.com"))
        kind = GitHostKind::GitLab;
    else if (host == QStringLiteral("codeberg.org"))
        kind = GitHostKind::Codeberg;
    else
        return std::nullopt;

    ParsedGitRepo repo;
    repo.kind = kind;
    repo.host = host;
    repo.repoSlug = stripRepoSuffix(match.captured(1) + QLatin1Char('/') + match.captured(2));
    repo.webUrl = QStringLiteral("https://%1/%2").arg(repo.host, repo.repoSlug);
    return repo;
}
}

QString ParsedGitRepo::providerId() const
{
    switch (kind) {
    case GitHostKind::GitLab:
        return QStringLiteral("gitlab");
    case GitHostKind::Codeberg:
        return QStringLiteral("codeberg");
    case GitHostKind::GitHub:
    default:
        return QStringLiteral("github");
    }
}

QString GitHost::hostForKind(GitHostKind kind)
{
    switch (kind) {
    case GitHostKind::GitLab:
        return QStringLiteral("gitlab.com");
    case GitHostKind::Codeberg:
        return QStringLiteral("codeberg.org");
    case GitHostKind::GitHub:
    default:
        return QStringLiteral("github.com");
    }
}

QString GitHost::projectId(const ParsedGitRepo &repo)
{
    return QStringLiteral("%1:%2").arg(repo.providerId(), repo.repoSlug);
}

QString GitHost::canonicalWebUrl(const ParsedGitRepo &repo)
{
    if (!repo.webUrl.isEmpty())
        return repo.webUrl;
    return QStringLiteral("https://%1/%2").arg(repo.host, repo.repoSlug);
}

bool GitHost::sameRepository(const ParsedGitRepo &a, const ParsedGitRepo &b)
{
    return a.host.compare(b.host, Qt::CaseInsensitive) == 0
            && a.repoSlug.compare(b.repoSlug, Qt::CaseInsensitive) == 0;
}

bool GitHost::sameRepository(const QString &urlA, const QString &urlB)
{
    const std::optional<ParsedGitRepo> a = parseUrl(urlA);
    const std::optional<ParsedGitRepo> b = parseUrl(urlB);
    if (!a || !b)
        return false;
    return sameRepository(*a, *b);
}

std::optional<ParsedGitRepo> GitHost::parseUrl(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    static const QRegularExpression sshRx(
        QStringLiteral(R"(^git@([^:]+):(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch sshMatch = sshRx.match(trimmed);
    if (sshMatch.hasMatch()) {
        const QString host = sshMatch.captured(1).toLower();
        const QString slug = stripRepoSuffix(sshMatch.captured(2));
        if (slug.contains(QLatin1Char('/'))) {
            ParsedGitRepo repo;
            repo.host = host;
            repo.repoSlug = slug;
            repo.webUrl = QStringLiteral("https://%1/%2").arg(host, slug);
            if (host == QStringLiteral("github.com"))
                repo.kind = GitHostKind::GitHub;
            else if (host == QStringLiteral("gitlab.com"))
                repo.kind = GitHostKind::GitLab;
            else if (host == QStringLiteral("codeberg.org"))
                repo.kind = GitHostKind::Codeberg;
            else
                return std::nullopt;
            return repo;
        }
    }

    QUrl url(trimmed);
    if (!url.isValid() || url.scheme().isEmpty()) {
        url = QUrl(QStringLiteral("https://") + trimmed);
    }
    if (!url.isValid() || url.host().isEmpty())
        return std::nullopt;

    return parseHttpsRepo(url);
}
