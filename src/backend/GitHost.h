#pragma once

#include <QString>
#include <optional>

enum class GitHostKind
{
    GitHub,
    GitLab,
    Codeberg,
};

struct ParsedGitRepo
{
    GitHostKind kind = GitHostKind::GitHub;
    QString host;
    QString repoSlug;
    QString webUrl;

    QString providerId() const;
};

class GitHost
{
public:
    static std::optional<ParsedGitRepo> parseUrl(const QString &input);
    static QString projectId(const ParsedGitRepo &repo);
    static QString hostForKind(GitHostKind kind);
    static QString canonicalWebUrl(const ParsedGitRepo &repo);
    static bool sameRepository(const ParsedGitRepo &a, const ParsedGitRepo &b);
    static bool sameRepository(const QString &urlA, const QString &urlB);
};
