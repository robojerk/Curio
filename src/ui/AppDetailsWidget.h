#pragma once

#include <QWidget>

#include "models/AppInfo.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QNetworkAccessManager;
class QTextBrowser;
class QTimer;
class FlatpakBackend;

class AppDetailsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppDetailsWidget(FlatpakBackend *backend, QWidget *parent = nullptr);

    void setApp(const AppInfo &info);
    void setInstalled(bool installed);
    void applyMetadataPatch(const AppInfo &patch);

signals:
    void backClicked();
    void installRequested(const AppInfo &info);

private slots:
    void onInstallClicked();
    void onLaunchClicked();
    void onRemoveClicked();
    void onUpdateClicked();
    void onManageClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateButtonStates();
    void setInstallInProgress(bool inProgress, int progress = -1);
    void cancelPendingLoads();
    void layoutInstallProgress();
    void tickInstallProgress();

    AppInfo m_info;
    FlatpakBackend *m_backend = nullptr;

    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_idLabel = nullptr;
    QLabel *m_originLabel = nullptr;
    QLabel *m_developerLabel = nullptr;
    QLabel *m_licenseLabel = nullptr;
    QTextBrowser *m_linksView = nullptr;
    QLabel *m_tagsLabel = nullptr;
    QLabel *m_screenshotsPlaceholder = nullptr;
    QWidget *m_screenshotsContainer = nullptr;
    QScrollArea *m_screenshotsScroll = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPushButton *m_installButton = nullptr;
    QProgressBar *m_installProgress = nullptr;
    QTimer *m_installProgressTimer = nullptr;
    int m_installProgressValue = 0;
    bool m_installInProgress = false;
    quint64 m_loadGeneration = 0;
    QPushButton *m_launchButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_updateButton = nullptr;
    QPushButton *m_manageButton = nullptr;
};
