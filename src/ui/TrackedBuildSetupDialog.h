#pragma once

#include "backend/TrackedBuildMatcher.h"
#include "models/AppInfo.h"
#include "models/TrackedBuild.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class FlatpakBackend;
class TrackedBuildSource;

class TrackedBuildSetupDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode
    {
        Add,
        Edit,
    };

    explicit TrackedBuildSetupDialog(FlatpakBackend *backend,
                                     Mode mode,
                                     const TrackedBuildProject &existing = {},
                                     const QVector<AppInfo> &installedApps = {},
                                     const TrackedBuildSetupHints &hints = {},
                                     QWidget *parent = nullptr);

    TrackedBuildProject resultProject() const { return m_project; }

signals:
    void installRequested(const TrackedBuildProject &project,
                          const QString &assetUrl,
                          const QString &linkedAppId,
                          const QString &releaseTag);

private slots:
    void onFetchClicked();
    void onSaveClicked();
    void onInstallLatestClicked();
    void updatePreview();

private:
    void setBusy(bool busy);
    void applySuggestions(const TrackedBuildSuggestions &suggestions);
    void applyInstalledAppMatches(const ParsedGitRepo &repo);
    void populateFromProject(const TrackedBuildProject &project);
    bool validateFields(QString *errorMessage) const;
    TrackedBuildProject buildProjectFromFields() const;
    bool finalizeProjectFromFields(TrackedBuildProject *project, QString *errorMessage) const;
    void updateInstallLatestButton();
    std::optional<ParsedGitRepo> currentParsedRepo() const;

    Mode m_mode;
    FlatpakBackend *m_backend = nullptr;
    TrackedBuildSource *m_source = nullptr;
    TrackedBuildProject m_project;
    QVector<TrackedBuildRelease> m_releases;
    QVector<AppInfo> m_installedApps;

    QLabel *m_bannerLabel = nullptr;
    QLabel *m_matchedAppLabel = nullptr;
    QComboBox *m_matchedAppCombo = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_repoLabel = nullptr;
    QLineEdit *m_linkedAppIdEdit = nullptr;
    QCheckBox *m_includePrereleasesCheck = nullptr;
    QLineEdit *m_assetFilterRegexEdit = nullptr;
    QLineEdit *m_releaseTitleFilterRegexEdit = nullptr;
    QLineEdit *m_excludeRegexEdit = nullptr;
    QCheckBox *m_includeZipCheck = nullptr;
    QLineEdit *m_zippedFlatpakRegexEdit = nullptr;
    QCheckBox *m_autoArchCheck = nullptr;
    QGroupBox *m_advancedGroup = nullptr;
    QTableWidget *m_previewTable = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_fetchButton = nullptr;
    QPushButton *m_installLatestButton = nullptr;
    QPushButton *m_saveButton = nullptr;
};
