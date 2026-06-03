#pragma once

#include <QString>

class CurioSettings
{
public:
    static CurioSettings &instance();

    QString settingsPath() const;

    QString githubPat() const;
    QString gitlabPat() const;
    QString codebergPat() const;

    /** Settings token, or GITHUB_TOKEN / GH_TOKEN when unset. */
    QString effectiveGithubPat() const;
    QString effectiveGitlabPat() const;
    QString effectiveCodebergPat() const;

    void setGithubPat(const QString &token);
    void setGitlabPat(const QString &token);
    void setCodebergPat(const QString &token);

    void load();
    void save() const;

private:
    CurioSettings() = default;

    QString m_path;
    QString m_githubPat;
    QString m_gitlabPat;
    QString m_codebergPat;
    bool m_loaded = false;
};
