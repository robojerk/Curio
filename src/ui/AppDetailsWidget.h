#pragma once

#include <QWidget>

#include "models/AppInfo.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QNetworkAccessManager;
class QTextBrowser;
class FlatpakBackend;

class AppDetailsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppDetailsWidget(FlatpakBackend *backend, QWidget *parent = nullptr);

    void setApp(const AppInfo &info);

signals:
    void backClicked();

private slots:
    void onInstallClicked();
    void onRemoveClicked();
    void onUpdateClicked();
    void onManageClicked();

private:
    void updateButtonStates();

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
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_updateButton = nullptr;
    QPushButton *m_manageButton = nullptr;
};
