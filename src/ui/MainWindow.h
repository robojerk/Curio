#pragma once

#include <QMainWindow>
#include <QVector>

#include "models/AppInfo.h"

class QLineEdit;
class QListView;
class QListWidget;
class QLabel;
class QPushButton;
class QScrollArea;
class QResizeEvent;
class QStackedWidget;
class QTabBar;
class QTimer;
class QWidget;

class AppDetailsWidget;
class AppListModel;
class OperationModel;
class FlatpakBackend;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onSearchDebounceFired();
    void onTopTabChanged(int index);
    void onFlathubFeedChanged(int index);
    void onSettingsSectionChanged(int row);
    void showDetailsForApp(const QString &appId);
    void onExploreInstallRequested(const QString &appId);
    void onInstalledUninstallRequested(const QString &appId);
    void onCheckForUpdatesTriggered();
    void onLoginWithFlathubTriggered();
    void onClearLeftoverUserDataTriggered();
    void onRefreshRemotesTriggered();
    void onAddRemoteTriggered();
    void onRemoveRemoteTriggered();

private:
    void setupUi();
    void connectBackend();
    void rebuildExploreCards();
    void rebuildInstalledRows();
    void refreshAllData();
    void refreshLeftoverDataPane();
    void refreshFeaturedBanner(const QVector<AppInfo> &apps);
    void refreshFlathubCategoriesPane();
    void rebuildFlathubCategoryCards();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_bannerWidget = nullptr;
    QLabel *m_bannerTitleLabel = nullptr;
    QLabel *m_bannerSummaryLabel = nullptr;
    QLabel *m_bannerIconLabel = nullptr;
    QWidget *m_topNav = nullptr;
    QPushButton *m_flathubButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_installedButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QTabBar *m_flathubFeedTabs = nullptr;
    QStackedWidget *m_mainContent = nullptr;
    QWidget *m_exploreContainer = nullptr;
    QWidget *m_flathubCategoriesContainer = nullptr;
    QListWidget *m_flathubCategoriesList = nullptr;
    QWidget *m_flathubCategoryCardsContainer = nullptr;
    QWidget *m_installedContainer = nullptr;
    QPushButton *m_checkUpdatesButton = nullptr;
    QWidget *m_settingsPage = nullptr;
    QListWidget *m_settingsNavList = nullptr;
    QStackedWidget *m_settingsDetailStack = nullptr;
    QLabel *m_leftoverDataPathLabel = nullptr;
    QListWidget *m_leftoverDataList = nullptr;
    QLabel *m_leftoverDataTotalLabel = nullptr;
    QPushButton *m_leftoverDataClearButton = nullptr;
    QListWidget *m_remotesList = nullptr;
    QLineEdit *m_remoteNameEdit = nullptr;
    QLineEdit *m_remoteUrlEdit = nullptr;
    QLabel *m_remoteStatusLabel = nullptr;
    QPushButton *m_removeRemoteButton = nullptr;
    QListView *m_operationsView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    AppDetailsWidget *m_detailsWidget = nullptr;

    AppListModel *m_exploreModel = nullptr;
    AppListModel *m_installedModel = nullptr;
    OperationModel *m_operationModel = nullptr;
    FlatpakBackend *m_backend = nullptr;
    QString m_currentDetailsAppId;
    QString m_lastSearchQuery;
    QTimer *m_searchDebounceTimer = nullptr;
    int m_topSectionIndex = 0;
    bool m_delaySearchEnabled = true;
    int m_searchDebounceMs = 1500;
    QVector<AppInfo> m_flathubSuggestions;
    QVector<AppInfo> m_flathubTrending;
    QVector<AppInfo> m_flathubPopular;
    QVector<AppInfo> m_flathubRecent;
    QVector<AppInfo> m_flathubUpdated;
    QVector<AppInfo> m_flathubCategoryPool;
    QString m_selectedFlathubCategory;
};
