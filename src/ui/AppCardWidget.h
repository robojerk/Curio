#pragma once

#include <QFrame>

#include "models/AppInfo.h"

class QLabel;
class QMouseEvent;
class QNetworkAccessManager;
class QProgressBar;
class QPushButton;
class QTimer;

class AppCardWidget : public QFrame
{
    Q_OBJECT
public:
    static constexpr int PreferredWidth = 320;
    static constexpr int PreferredHeight = 100;

    explicit AppCardWidget(QWidget *parent = nullptr);

    void setApp(const AppInfo &info, bool refreshIconNow = true);
    void patchIcon(const AppInfo &info);
    void refreshIcon();
    void setInstallInProgress(bool inProgress, int progress = -1);
    void setInstalled(bool installed);

    QString appId() const { return m_info.id; }

signals:
    void openDetailsRequested();
    void installRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyInstallUiState();
    void layoutInstallProgress();
    void tickInstallProgress();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QLabel *m_installedBadge = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPushButton *m_installButton = nullptr;
    QProgressBar *m_installProgress = nullptr;
    QTimer *m_installProgressTimer = nullptr;
    int m_installProgressValue = 0;
    bool m_installInProgress = false;
    AppInfo m_info;
};
