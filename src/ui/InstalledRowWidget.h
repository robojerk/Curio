#pragma once

#include <QWidget>

#include "models/AppInfo.h"

class QLabel;
class QMouseEvent;
class QProgressBar;
class QPushButton;
class QTimer;

class InstalledRowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InstalledRowWidget(QWidget *parent = nullptr);

    void setApp(const AppInfo &info);
    void setTrackGitRepoLink(bool enabled, const QString &label);
    void setTrackedUpdateAvailable(bool available, const QString &releaseTag);
    void setRemoteUpdateAvailable(bool available);
    void setUpdateInProgress(bool inProgress, const QString &statusText = QString(), int progress = -1);
    void setUninstallInProgress(bool inProgress);

    QString appId() const { return m_info.id; }

signals:
    void openDetailsRequested();
    void updateRequested();
    void uninstallRequested();
    void trackGitRepoRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateSourceLabel();
    void updateSummaryLabel();
    void syncUpdateButtonVisibility();
    void layoutUninstallProgress();
    void layoutUpdateProgress();
    void tickUninstallProgress();
    void tickUpdateProgress();

    bool m_trackGitLinkEnabled = false;
    QString m_trackLinkLabel;
    bool m_trackedUpdateAvailable = false;
    QString m_trackedUpdateTag;
    bool m_remoteUpdateAvailable = false;
    bool m_updateInProgress = false;

    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QPushButton *m_updateButton = nullptr;
    QProgressBar *m_updateProgress = nullptr;
    QTimer *m_updateProgressTimer = nullptr;
    int m_updateProgressValue = 0;
    QPushButton *m_uninstallButton = nullptr;
    QProgressBar *m_uninstallProgress = nullptr;
    QTimer *m_uninstallProgressTimer = nullptr;
    int m_uninstallProgressValue = 0;
    AppInfo m_info;
};
