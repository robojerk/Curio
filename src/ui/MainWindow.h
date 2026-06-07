#pragma once

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QMainWindow>
#include <QVector>

#include "models/AppInfo.h"
#include "models/Operation.h"
#include "models/StoreTemplate.h"
#include "models/TrackedBuild.h"

#include "backend/TrackedBuildMatcher.h"

#include "ui/TrackedBuildSetupDialog.h"

class QLineEdit;
class QListView;
class QListWidget;
class QLabel;
class QHBoxLayout;
class QPushButton;
class QCheckBox;
class QScrollArea;
class QResizeEvent;
class QStackedWidget;
class QTabBar;
class QTimer;
class QWidget;

class AppCardWidget;
class AppDetailsWidget;
class RuntimeUpdatesRowWidget;
class AppListModel;
class InstalledRowWidget;
class OperationModel;
class FlatpakBackend;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void handleOpenFileRequest(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onSearchDebounceFired();
    void onTopTabChanged(int index);
    void onStoreTemplateChanged(int index);
    void onStoreFeedChanged(int index);
    void onSettingsSectionChanged(int row);
    void showDetailsForApp(const QString &appId);
    void presentAppDetailsPage(const AppInfo &info);
    void installStoreApp(const AppInfo &info);
    void onInstalledUninstallRequested(const QString &appId);
    void onCheckForUpdatesTriggered();
    void onUpdateAllTriggered();
    void startNextQueuedUpdate();
    void finishUpdateAll();
    bool appHasPendingUpdate(const AppInfo &app) const;
    void refreshUpdateAllButtonState();
    void onClearLeftoverUserDataTriggered();
    void onRefreshRemotesTriggered();
    void onAddRemoteTriggered();
    void onRemoveRemoteTriggered();
    void onTrackedBuildAdd();
    void onTrackedBuildEdit();
    void onTrackedBuildRemove();
    void onTrackedBuildRefresh();
    void onTrackedBuildSelectionChanged();
    void openTrackedBuildDialogForApp(const AppInfo &app);
    void onTrackedBuildInstallRequested(const TrackedBuildProject &project,
                                        const QString &assetUrl,
                                        const QString &linkedAppId,
                                        const QString &releaseTag);
    bool presentTrackedBuildSetupDialog(TrackedBuildSetupDialog::Mode mode,
                                        const TrackedBuildProject &existing = {},
                                        const TrackedBuildSetupHints &hints = {});

private:
    void setupUi();
    void connectBackend();
    void clearExploreGridLayout();
    void rebuildExploreCards();
    void scheduleExploreRebuild();
    void scheduleDeferredExploreIconRefresh();
    QVector<AppInfo> exploreAppsForFeedIndex(int feedIndex) const;
    void refreshCurrentStoreFeed();
    void scheduleExploreResizeReflow();
    bool tryRefreshExploreInPlace();
    QString exploreModelSignature(const QVector<AppInfo> &apps) const;
    QVector<AppInfo> filteredCategoryApps() const;
    void rebuildStoreCategoryCards();
    void scheduleCategoryResizeReflow();
    bool tryRefreshCategoryInPlace();
    void updateExploreSkeleton();
    void updateCategoryFeed();
    void rebuildInstalledRows();
    void refreshAllData();
    void refreshLeftoverDataPane();
    void refreshFeaturedBanner(const QVector<AppInfo> &apps);
    void refreshActiveBannerCard();
    void updateFeaturedBannerLayout();
    void scrollBannerBy(int direction);
    void refreshStoreCategoriesPane();
    StoreTemplate defaultStoreTemplate() const;
    QVector<StoreTemplate> allStoreTemplates() const;
    void updateStoreTemplatesForRemotes(const QVector<QPair<QString, QString>> &remotes);
    void rebuildStoreTemplateTabs();
    void applyActiveStoreTemplateUi();
    QString buildStorePageUrl(const AppInfo &app) const;
    void prepareDetailsAppInfo(AppInfo &info) const;
    void refreshTrackedBuildsList();
    void runDeferredStartup();
    void applyStoreDiscoveryData();
    void patchStoreCollectionsIcons(const QVector<AppInfo> &updates);
    void patchStoreCollectionsMetadata(const QVector<AppInfo> &updates);
    struct TrackedUpdateSummary {
        int trackedInstalledCount = 0;
        int trackedPendingCount = 0;
        int flatpakPendingCount = 0;
        int runtimePendingCount = 0;
        QStringList errors;
    };
    void refreshRuntimeUpdatesRow();
    TrackedUpdateSummary summarizeInstalledUpdates() const;
    void refreshInstalledUpdateBanner();
    void updateInstalledNavAttention();
    void finishCheckForUpdatesFeedback(bool userInitiated);
    void refreshInstalledRowUpdateState(InstalledRowWidget *row, const AppInfo &app);
    void refreshAllInstalledRowUpdateStates();
    void completePendingUpdateCheck(bool userInitiated);
    void setStoreFeedLoading(bool loading);
    void beginStoreRefresh(bool forceRefresh = false);
    void updateSearchHintLabel();
    QString storeExploreEmptyText() const;
    void purgeOrphanExploreWidgets();
    void updateInstalledRowOperation(const Operation &op, bool finished);
    void updateStoreCardOperation(const Operation &op, bool finished);
    void syncStoreFeedsInstalledState();
    void refreshStoreCardsInstalledUi();
    void pruneStaleStoreCards();
    AppCardWidget *storeCardForAppId(const QString &appId) const;
    AppInfo lookupStoreFeedApp(const QString &appId) const;
    QSet<QString> installedAppIds() const;
    void updateTrackedInstallFeedback(const Operation &op, bool finished);
    void beginTrackedInstallFeedback(const QString &appId, const QString &releaseTag);
    QVector<AppInfo> installedAppsSnapshot() const;
    void configureInstalledRowSource(InstalledRowWidget *row, const AppInfo &app);
    void refreshInstalledRowTrackedUpdate(InstalledRowWidget *row, const AppInfo &app);
    void patchInstalledTrackedBuildMetadata();
    void patchInstalledAppsMetadata(const QVector<AppInfo> &patches);

    bool m_deferredStartupDone = false;
    bool m_checkForUpdatesPending = false;
    int m_pendingUpdateCheckTasks = 0;
    bool m_installedNavPulseBright = false;
    bool m_storeFeedLoading = false;
    bool m_showExploreSkeleton = false;
    bool m_searchInProgress = false;
    QString m_appliedStoreRepoId;
    QHash<QString, InstalledRowWidget *> m_installedRowsByAppId;
    QHash<QString, QPointer<AppCardWidget>> m_storeCardsByAppId;
    QString m_trackedInstallFeedbackAppId;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_bannerHost = nullptr;
    QWidget *m_bannerWidget = nullptr;
    int m_bannerIconPixels = 72;
    QLabel *m_bannerEyebrowLabel = nullptr;
    QLabel *m_bannerTitleLabel = nullptr;
    QLabel *m_bannerSummaryLabel = nullptr;
    QLabel *m_bannerIconLabel = nullptr;
    QPushButton *m_bannerOpenButton = nullptr;
    QPushButton *m_bannerPrevButton = nullptr;
    QPushButton *m_bannerNextButton = nullptr;
    QTimer *m_bannerAutoScrollTimer = nullptr;
    QVector<AppInfo> m_bannerApps;
    int m_bannerCurrentIndex = 0;
    QWidget *m_topNav = nullptr;
    QPushButton *m_storeButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_installedButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QTabBar *m_storeTemplateTabs = nullptr;
    QWidget *m_storeTemplateTabsHost = nullptr;
    QTabBar *m_storeFeedTabs = nullptr;
    QLabel *m_storeStatusLabel = nullptr;
    QStackedWidget *m_mainContent = nullptr;
    QScrollArea *m_exploreScroll = nullptr;
    QWidget *m_exploreContainer = nullptr;
    QWidget *m_storeCategoriesContainer = nullptr;
    QListWidget *m_storeCategoriesList = nullptr;
    QWidget *m_storeCategoryCardsContainer = nullptr;
    QWidget *m_installedContainer = nullptr;
    QPushButton *m_checkUpdatesButton = nullptr;
    QPushButton *m_updateAllButton = nullptr;
    QLabel *m_checkUpdatesStatusLabel = nullptr;
    RuntimeUpdatesRowWidget *m_runtimeUpdatesRow = nullptr;
    QVector<AppInfo> m_runtimeUpdateQueue;
    QString m_runtimeUpdateCurrentRef;
    QStringList m_updateAllQueue;
    QString m_updateAllCurrentAppId;
    bool m_updateAllActive = false;
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
    QListWidget *m_trackedBuildsList = nullptr;
    QLabel *m_trackedStatusLabel = nullptr;
    QPushButton *m_trackedAddButton = nullptr;
    QPushButton *m_trackedEditButton = nullptr;
    QPushButton *m_trackedRemoveButton = nullptr;
    QLineEdit *m_githubPatEdit = nullptr;
    QLineEdit *m_gitlabPatEdit = nullptr;
    QLineEdit *m_codebergPatEdit = nullptr;
    QListView *m_operationsView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_searchHintLabel = nullptr;
    AppDetailsWidget *m_detailsWidget = nullptr;

    AppListModel *m_exploreModel = nullptr;
    AppListModel *m_categoryModel = nullptr;
    QString m_lastExploreModelSignature;
    int m_lastExploreColumnCount = 0;
    bool m_exploreRebuildScheduled = false;
    bool m_exploreIconRefreshScheduled = false;
    QTimer *m_exploreIconRefreshTimer = nullptr;
    QVector<AppCardWidget *> m_exploreIconRefreshQueue;
    int m_exploreIconRefreshIndex = 0;
    QTimer *m_exploreResizeTimer = nullptr;
    QString m_lastCategorySignature;
    int m_lastCategoryColumnCount = 0;
    QTimer *m_categoryResizeTimer = nullptr;
    AppListModel *m_installedModel = nullptr;
    OperationModel *m_operationModel = nullptr;
    FlatpakBackend *m_backend = nullptr;
    QString m_currentDetailsAppId;
    AppInfo m_detailsPageAppInfo;
    QString m_lastSearchQuery;
    QTimer *m_searchDebounceTimer = nullptr;
    QTimer *m_installedNavPulseTimer = nullptr;
    int m_topSectionIndex = 0;
    bool m_delaySearchEnabled = true;
    int m_searchDebounceMs = 1500;
    QVector<StoreTemplate> m_storeTemplates;
    QVector<TrackedBuildProject> m_trackedBuildProjects;
    StoreTemplate m_activeStoreTemplate;
    QVector<AppInfo> m_storeSuggestions;
    QVector<AppInfo> m_storeTrending;
    QVector<AppInfo> m_storePopular;
    QVector<AppInfo> m_storeRecent;
    QVector<AppInfo> m_storeUpdated;
    QVector<AppInfo> m_storeCategoryPool;
    QString m_selectedStoreCategory;
};
