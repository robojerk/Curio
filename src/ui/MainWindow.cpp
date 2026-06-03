#include "MainWindow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfoList>
#include <QFile>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QWidget>
#include <QStringList>
#include <QIcon>
#include <QColor>
#include <QSet>
#include <QMessageBox>
#include <QDialog>
#include <QUrl>
#include <QSplitter>
#include <QToolButton>

#include "backend/CurioSettings.h"
#include "backend/FlatpakBackend.h"
#include "backend/TrackedBuildClassifier.h"
#include "backend/TrackedBuildMatcher.h"
#include "models/AppListModel.h"
#include "models/OperationModel.h"
#include "ui/TrackedBuildSetupDialog.h"
#include "ui/AppCardWidget.h"
#include "ui/AppDetailsWidget.h"
#include "ui/InstalledRowWidget.h"

namespace {

class ToggleSplitter : public QSplitter
{
public:
    explicit ToggleSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSplitter(orientation, parent)
    {
        setChildrenCollapsible(true);
    }

protected:
    QSplitterHandle *createHandle() override
    {
        return new QSplitterHandle(orientation(), this);
    }
};

int exploreAvailableWidth(QWidget *container, const QGridLayout *grid)
{
    if (!container || !grid)
        return 0;
    const int margins = grid->contentsMargins().left() + grid->contentsMargins().right();
    int width = container->width() - margins;
    if (width >= AppCardWidget::PreferredWidth)
        return width;

    if (auto *scroll = qobject_cast<QScrollArea *>(container->parentWidget()))
        width = scroll->viewport()->width() - margins;
    return qMax(0, width);
}

int exploreColumnCount(int availableWidth, int spacing)
{
    const int minCardWidth = AppCardWidget::PreferredWidth;
    if (availableWidth <= 0)
        return 1;
    return qMax(1, (availableWidth + spacing) / (minCardWidth + spacing));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_storeTemplates = QVector<StoreTemplate>{defaultStoreTemplate()};
    m_activeStoreTemplate = m_storeTemplates.first();
    m_appliedStoreRepoId = m_activeStoreTemplate.repoId;
    m_backend = new FlatpakBackend(this);
    m_exploreModel = new AppListModel(this);
    m_installedModel = new AppListModel(this);
    m_operationModel = new OperationModel(this);

    setupUi();
    connectBackend();
    m_backend->emitCachedDiscoveryData();
    if (m_storeTemplateTabs) {
        m_storeTemplateTabs->blockSignals(true);
        m_storeTemplateTabs->setCurrentIndex(0);
        m_storeTemplateTabs->blockSignals(false);
    }
    if (m_storeFeedTabs) {
        m_storeFeedTabs->blockSignals(true);
        m_storeFeedTabs->setCurrentIndex(0);
        m_storeFeedTabs->blockSignals(false);
    }
    onTopTabChanged(0);
}

void MainWindow::handleOpenFileRequest(const QString &path)
{
    QString localPath = path.trimmed();
    if (localPath.isEmpty())
        return;

    if (localPath.startsWith(QStringLiteral("file://"))) {
        const QUrl url(localPath);
        if (url.isValid())
            localPath = url.toLocalFile();
    }

    QFileInfo info(localPath);
    if (!info.exists() || !info.isFile())
        return;

    const QString suffix = info.suffix().toLower();
    if (suffix != QStringLiteral("flatpak") && suffix != QStringLiteral("flatpakref"))
        return;

    const auto choice = QMessageBox::question(
        this,
        tr("Install Flatpak Package"),
        tr("Install package from file?\n%1").arg(info.fileName()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (choice != QMessageBox::Yes)
        return;

    onTopTabChanged(2); // Installed Apps
    m_backend->installBundle(info.absoluteFilePath());
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 8, 10, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(central);

    // Host for main content, centered with a max width so banner and cards align
    auto *contentHost = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    contentHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Let the host expand to the full available width so the cards
    // can lay out in multiple columns; inner layouts handle centering.
    rootLayout->addWidget(contentHost);

    m_bannerWidget = new QFrame(this);
    m_bannerWidget->setObjectName(QStringLiteral("featuredBanner"));
    m_bannerWidget->setMinimumHeight(100);
    m_bannerWidget->setMaximumHeight(140);
    m_bannerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_bannerWidget->setStyleSheet(QStringLiteral(
        "#featuredBanner {"
        "  background-color: #2ea56b;"
        "  border-radius: 12px;"
        "}"
    ));
    auto *bannerLayout = new QHBoxLayout(m_bannerWidget);
    bannerLayout->setContentsMargins(20, 16, 20, 16);
    bannerLayout->setSpacing(12);

    m_bannerPrevButton = new QPushButton(QStringLiteral("<"), m_bannerWidget);
    m_bannerPrevButton->setFixedSize(36, 36);
    m_bannerPrevButton->setCursor(Qt::PointingHandCursor);
    m_bannerPrevButton->setToolTip(tr("Previous banners"));
    m_bannerPrevButton->setObjectName(QStringLiteral("bannerArrowButton"));
    bannerLayout->addWidget(m_bannerPrevButton, 0, Qt::AlignVCenter);

    auto *bannerTextLayout = new QVBoxLayout;
    m_bannerEyebrowLabel = new QLabel(m_activeStoreTemplate.bannerEyebrow, m_bannerWidget);
    m_bannerEyebrowLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-weight: 600;"));
    m_bannerTitleLabel = new QLabel(tr("Discover apps"), m_bannerWidget);
    m_bannerTitleLabel->setStyleSheet(QStringLiteral("color: white; font-size: 24px; font-weight: 700;"));
    m_bannerSummaryLabel = new QLabel(m_activeStoreTemplate.bannerEmptySummary, m_bannerWidget);
    m_bannerSummaryLabel->setWordWrap(true);
    m_bannerSummaryLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92);"));
    bannerTextLayout->addWidget(m_bannerEyebrowLabel);
    bannerTextLayout->addWidget(m_bannerTitleLabel);
    bannerTextLayout->addWidget(m_bannerSummaryLabel);
    bannerTextLayout->addStretch();
    bannerLayout->addLayout(bannerTextLayout, 1);

    auto *rightBannerLayout = new QVBoxLayout;
    rightBannerLayout->setContentsMargins(0, 0, 0, 0);
    rightBannerLayout->setSpacing(8);
    m_bannerIconLabel = new QLabel(m_bannerWidget);
    m_bannerIconLabel->setFixedSize(96, 96);
    m_bannerIconLabel->setAlignment(Qt::AlignCenter);
    m_bannerIconLabel->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.08); border-radius: 12px;"));
    const QIcon defaultBannerIcon = QIcon::fromTheme(QStringLiteral("applications-other"));
    m_bannerIconLabel->setPixmap(defaultBannerIcon.pixmap(72, 72));
    m_bannerOpenButton = new QPushButton(tr("Open"), m_bannerWidget);
    m_bannerOpenButton->setCursor(Qt::PointingHandCursor);
    m_bannerOpenButton->setFixedWidth(90);
    m_bannerOpenButton->setObjectName(QStringLiteral("bannerOpenButton"));
    rightBannerLayout->addWidget(m_bannerIconLabel, 0, Qt::AlignRight);
    rightBannerLayout->addWidget(m_bannerOpenButton, 0, Qt::AlignRight);
    rightBannerLayout->addStretch();
    bannerLayout->addLayout(rightBannerLayout, 0);

    m_bannerNextButton = new QPushButton(QStringLiteral(">"), m_bannerWidget);
    m_bannerNextButton->setFixedSize(36, 36);
    m_bannerNextButton->setCursor(Qt::PointingHandCursor);
    m_bannerNextButton->setToolTip(tr("Next banners"));
    m_bannerNextButton->setObjectName(QStringLiteral("bannerArrowButton"));
    bannerLayout->addWidget(m_bannerNextButton, 0, Qt::AlignVCenter);

    m_topNav = new QWidget(this);
    m_topNav->setObjectName(QStringLiteral("topNavHost"));
    auto *topNavLayout = new QHBoxLayout(m_topNav);
    topNavLayout->setContentsMargins(0, 0, 0, 0);
    topNavLayout->setSpacing(8);
    topNavLayout->addStretch();
    m_storeButton = new QPushButton(m_activeStoreTemplate.displayName, m_topNav);
    m_searchButton = new QPushButton(tr("Search"), m_topNav);
    m_installedButton = new QPushButton(tr("Installed Apps"), m_topNav);
    m_settingsButton = new QPushButton(tr("Settings"), m_topNav);
    for (QPushButton *btn : {m_storeButton, m_searchButton, m_installedButton, m_settingsButton}) {
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setProperty("topNav", true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(36);
    }
    m_topNav->setStyleSheet(QStringLiteral(
        "#topNavHost QPushButton[topNav=\"true\"] {"
        "  border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 18px;"
        "  padding: 6px 16px;"
        "  background-color: rgba(255,255,255,0.05);"
        "  color: rgba(245,245,245,0.92);"
        "  font-weight: 500;"
        "}"
        "#topNavHost QPushButton[topNav=\"true\"]:hover {"
        "  background-color: rgba(255,255,255,0.10);"
        "  border-color: rgba(255,255,255,0.24);"
        "}"
        "#topNavHost QPushButton[topNav=\"true\"]:pressed {"
        "  background-color: rgba(255,255,255,0.16);"
        "}"
        "#topNavHost QPushButton[topNav=\"true\"]:checked {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5f63d6, stop:1 #2aa96b);"
        "  border-color: rgba(255,255,255,0.32);"
        "  color: white;"
        "  font-weight: 650;"
        "}"
        "#topNavHost QPushButton[topNav=\"true\"][updatesPending=\"true\"] {"
        "  border-color: rgba(232, 180, 80, 0.55);"
        "  background-color: rgba(232, 180, 80, 0.10);"
        "}"
        "#topNavHost QPushButton[topNav=\"true\"][updatesPending=\"true\"][pulseBright=\"true\"] {"
        "  border-color: rgba(255, 210, 100, 0.95);"
        "  background-color: rgba(232, 180, 80, 0.24);"
        "  color: rgba(255, 245, 220, 0.98);"
        "}"
    ));
    m_storeButton->setChecked(true);
    topNavLayout->addWidget(m_storeButton);
    topNavLayout->addWidget(m_searchButton);
    topNavLayout->addWidget(m_installedButton);
    topNavLayout->addWidget(m_settingsButton);
    topNavLayout->addStretch();
    contentLayout->addWidget(m_topNav);

    // Banner slightly narrower than full content width (approx ~70% on large windows)
    auto *bannerHost = new QWidget(this);
    auto *bannerHostLayout = new QHBoxLayout(bannerHost);
    bannerHostLayout->setContentsMargins(0, 0, 0, 0);
    bannerHostLayout->setSpacing(0);
    bannerHostLayout->addStretch();
    m_bannerWidget->setMaximumWidth(900);
    bannerHostLayout->addWidget(m_bannerWidget);
    bannerHostLayout->addStretch();
    contentLayout->addWidget(bannerHost);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search apps…"));
    contentLayout->addWidget(m_searchEdit);

    m_storeTemplateTabs = new QTabBar(this);
    m_storeTemplateTabs->setObjectName(QStringLiteral("storeTemplateTabs"));
    m_storeTemplateTabs->setExpanding(false);
    m_storeTemplateTabs->setDocumentMode(true);
    m_storeTemplateTabs->setDrawBase(false);
    m_storeTemplateTabs->setUsesScrollButtons(false);
    m_storeTemplateTabs->setStyleSheet(QStringLiteral(
        "QTabBar#storeTemplateTabs::tab {"
        "  border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 14px;"
        "  padding: 4px 12px;"
        "  margin-right: 6px;"
        "  margin-bottom: 2px;"
        "  background-color: rgba(255,255,255,0.03);"
        "  color: rgba(236,236,236,0.85);"
        "}"
        "QTabBar#storeTemplateTabs::tab:selected {"
        "  background-color: rgba(42,169,107,0.28);"
        "  border-color: rgba(111,220,163,0.62);"
        "  color: white;"
        "}"
    ));
    m_storeTemplateTabsHost = new QWidget(this);
    auto *storeTemplateTabsLayout = new QHBoxLayout(m_storeTemplateTabsHost);
    storeTemplateTabsLayout->setContentsMargins(0, 0, 0, 0);
    storeTemplateTabsLayout->setSpacing(0);
    storeTemplateTabsLayout->addStretch();
    storeTemplateTabsLayout->addWidget(m_storeTemplateTabs);
    storeTemplateTabsLayout->addStretch();
    contentLayout->addWidget(m_storeTemplateTabsHost);
    rebuildStoreTemplateTabs();

    m_storeFeedTabs = new QTabBar(this);
    m_storeFeedTabs->setObjectName(QStringLiteral("storeFeedTabs"));
    for (const QString &label : m_activeStoreTemplate.feedLabels)
        m_storeFeedTabs->addTab(label);
    m_storeFeedTabs->setExpanding(false);
    m_storeFeedTabs->setDocumentMode(true);
    m_storeFeedTabs->setDrawBase(false);
    m_storeFeedTabs->setUsesScrollButtons(false);
    m_storeFeedTabs->setStyleSheet(QStringLiteral(
        "QTabBar#storeFeedTabs::tab {"
        "  border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 14px;"
        "  padding: 6px 14px;"
        "  margin-right: 6px;"
        "  margin-bottom: 2px;"
        "  background-color: rgba(255,255,255,0.04);"
        "  color: rgba(236,236,236,0.88);"
        "  font-weight: 500;"
        "}"
        "QTabBar#storeFeedTabs::tab:hover {"
        "  background-color: rgba(255,255,255,0.09);"
        "  border-color: rgba(255,255,255,0.22);"
        "}"
        "QTabBar#storeFeedTabs::tab:selected {"
        "  background-color: rgba(94,102,219,0.30);"
        "  border-color: rgba(122,180,255,0.62);"
        "  color: white;"
        "  font-weight: 650;"
        "}"
    ));

    auto *storeFeedTabsHost = new QWidget(this);
    auto *storeFeedTabsLayout = new QHBoxLayout(storeFeedTabsHost);
    storeFeedTabsLayout->setContentsMargins(0, 0, 0, 0);
    storeFeedTabsLayout->setSpacing(0);
    storeFeedTabsLayout->addStretch();
    storeFeedTabsLayout->addWidget(m_storeFeedTabs);
    storeFeedTabsLayout->addStretch();
    contentLayout->addWidget(storeFeedTabsHost);

    m_storeStatusLabel = new QLabel(this);
    m_storeStatusLabel->setWordWrap(true);
    m_storeStatusLabel->setVisible(false);
    m_storeStatusLabel->setStyleSheet(QStringLiteral(
        "padding: 8px 12px;"
        "border: 1px solid rgba(220,120,120,0.45);"
        "border-radius: 8px;"
        "background: rgba(220,120,120,0.10);"
        "color: rgba(255,220,220,0.95);"
    ));
    contentLayout->addWidget(m_storeStatusLabel);

    const QString bannerButtonStyle = QStringLiteral(
        "QPushButton#bannerArrowButton {"
        "  border: 1px solid rgba(255,255,255,0.20);"
        "  border-radius: 18px;"
        "  background-color: rgba(20,24,32,0.32);"
        "  color: white;"
        "  font-weight: 700;"
        "}"
        "QPushButton#bannerArrowButton:hover {"
        "  background-color: rgba(20,24,32,0.48);"
        "  border-color: rgba(255,255,255,0.34);"
        "}"
        "QPushButton#bannerOpenButton {"
        "  border: 1px solid rgba(255,255,255,0.20);"
        "  border-radius: 16px;"
        "  background-color: rgba(20,24,32,0.36);"
        "  color: white;"
        "  padding: 6px 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#bannerOpenButton:hover {"
        "  background-color: rgba(20,24,32,0.52);"
        "  border-color: rgba(255,255,255,0.34);"
        "}"
    );
    m_bannerPrevButton->setStyleSheet(bannerButtonStyle);
    m_bannerNextButton->setStyleSheet(bannerButtonStyle);
    m_bannerOpenButton->setStyleSheet(bannerButtonStyle);

    m_stack = new QStackedWidget(this);
    contentLayout->addWidget(m_stack, 1);

    // Page 0: main content stack (Flathub explore, Installed, Operations)
    m_mainContent = new QStackedWidget(this);

    // Explore tab: scroll area with grid of cards
    auto *exploreScroll = new QScrollArea(this);
    exploreScroll->setWidgetResizable(true);
    exploreScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_exploreContainer = new QWidget(this);
    auto *exploreGrid = new QGridLayout(m_exploreContainer);
    exploreGrid->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    exploreGrid->setHorizontalSpacing(12);
    exploreGrid->setVerticalSpacing(12);
    exploreGrid->setContentsMargins(8, 8, 8, 8);
    m_exploreContainer->setLayout(exploreGrid);
    m_exploreContainer->setMinimumSize(0, 0);
    m_exploreContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    exploreScroll->setWidget(m_exploreContainer);
    exploreScroll->setMinimumHeight(120);
    m_mainContent->addWidget(exploreScroll);

    // Flathub categories tab content
    auto *categoriesScroll = new QScrollArea(this);
    categoriesScroll->setWidgetResizable(true);
    categoriesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_storeCategoriesContainer = new QWidget(this);
    auto *categoriesLayout = new QVBoxLayout(m_storeCategoriesContainer);
    categoriesLayout->setContentsMargins(8, 8, 8, 8);
    categoriesLayout->setSpacing(10);

    auto *splitter = new ToggleSplitter(Qt::Horizontal, m_storeCategoriesContainer);
    splitter->setHandleWidth(6);

    auto *leftPane = new QFrame(splitter);
    leftPane->setFrameShape(QFrame::StyledPanel);
    leftPane->setStyleSheet(QStringLiteral("QFrame { border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; }"));
    auto *leftPaneLayout = new QVBoxLayout(leftPane);
    leftPaneLayout->setContentsMargins(10, 10, 10, 10);
    leftPaneLayout->setSpacing(6);
    auto *leftTitle = new QLabel(tr("Categories"), leftPane);
    leftTitle->setStyleSheet(QStringLiteral("font-weight: 600; color: rgba(240,240,245,0.92);"));
    leftPaneLayout->addWidget(leftTitle);
    m_storeCategoriesList = new QListWidget(leftPane);
    m_storeCategoriesList->setMinimumWidth(220);
    m_storeCategoriesList->setUniformItemSizes(true);
    m_storeCategoriesList->setSpacing(4);
    m_storeCategoriesList->setAlternatingRowColors(false);
    m_storeCategoriesList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  border: none;"
        "  background-color: transparent;"
        "  color: rgba(235,235,240,0.94);"
        "}"
        "QListWidget::item {"
        "  padding: 6px 10px;"
        "  margin: 2px 0;"
        "  border-radius: 8px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(94,102,219,0.55);"
        "  color: white;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: rgba(255,255,255,0.06);"
        "}"
    ));
    leftPaneLayout->addWidget(m_storeCategoriesList, 1);

    auto *rightPane = new QFrame(splitter);
    rightPane->setFrameShape(QFrame::StyledPanel);
    rightPane->setStyleSheet(QStringLiteral("QFrame { border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; }"));
    auto *rightPaneLayout = new QVBoxLayout(rightPane);
    rightPaneLayout->setContentsMargins(10, 10, 10, 10);
    rightPaneLayout->setSpacing(10);
    auto *rightTitle = new QLabel(tr("Apps"), rightPane);
    rightTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    rightPaneLayout->addWidget(rightTitle);
    auto *categoryAppsScroll = new QScrollArea(rightPane);
    categoryAppsScroll->setWidgetResizable(true);
    categoryAppsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_storeCategoryCardsContainer = new QWidget(categoryAppsScroll);
    m_storeCategoryCardsContainer->setMinimumSize(0, 0);
    m_storeCategoryCardsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *categoryCardsGrid = new QGridLayout(m_storeCategoryCardsContainer);
    categoryCardsGrid->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    categoryCardsGrid->setHorizontalSpacing(12);
    categoryCardsGrid->setVerticalSpacing(12);
    categoryCardsGrid->setContentsMargins(0, 0, 0, 0);
    categoryAppsScroll->setWidget(m_storeCategoryCardsContainer);
    rightPaneLayout->addWidget(categoryAppsScroll, 1);

    connect(m_storeCategoriesList, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        m_selectedStoreCategory = text.trimmed();
        rebuildStoreCategoryCards();
    });

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    categoriesLayout->addWidget(splitter, 1);
    categoriesScroll->setWidget(m_storeCategoriesContainer);
    m_mainContent->addWidget(categoriesScroll);

    // Installed tab: scroll area with list of row widgets
    auto *installedScroll = new QScrollArea(this);
    installedScroll->setWidgetResizable(true);
    installedScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_installedContainer = new QWidget(this);
    auto *installedLayout = new QVBoxLayout(m_installedContainer);
    installedLayout->setAlignment(Qt::AlignTop);
    m_checkUpdatesButton = new QPushButton(tr("Check for updates"), m_installedContainer);
    installedLayout->addWidget(m_checkUpdatesButton, 0, Qt::AlignLeft);
    m_checkUpdatesStatusLabel = new QLabel(m_installedContainer);
    m_checkUpdatesStatusLabel->setWordWrap(true);
    m_checkUpdatesStatusLabel->setStyleSheet(QStringLiteral("color: rgba(200,200,210,0.92);"));
    m_checkUpdatesStatusLabel->hide();
    installedLayout->addWidget(m_checkUpdatesStatusLabel);
    installedScroll->setWidget(m_installedContainer);
    m_mainContent->addWidget(installedScroll);

    // Operations tab
    auto *opsPage = new QWidget(this);
    auto *opsLayout = new QVBoxLayout(opsPage);
    m_operationsView = new QListView(this);
    m_operationsView->setModel(m_operationModel);
    opsLayout->addWidget(m_operationsView);
    m_mainContent->addWidget(opsPage);

    m_stack->addWidget(m_mainContent); // page 0: main content

    // Page 1: app details (in-window store page)
    m_detailsWidget = new AppDetailsWidget(m_backend, this);
    connect(m_detailsWidget, &AppDetailsWidget::backClicked, this, [this]() {
        m_stack->setCurrentIndex(0);
        onTopTabChanged(m_topSectionIndex);
    });
    m_stack->addWidget(m_detailsWidget);

    // Page 2: settings
    m_settingsPage = new QWidget(this);
    auto *settingsLayout = new QVBoxLayout(m_settingsPage);
    auto *settingsLabel = new QLabel(tr("Settings"), m_settingsPage);
    QFont settingsTitleFont = settingsLabel->font();
    settingsTitleFont.setBold(true);
    settingsTitleFont.setPointSizeF(settingsTitleFont.pointSizeF() + 2);
    settingsLabel->setFont(settingsTitleFont);
    settingsLayout->addWidget(settingsLabel);

    auto *hint = new QLabel(tr("Choose an action below."), m_settingsPage);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    settingsLayout->addWidget(hint);

    auto *settingsPanesLayout = new QHBoxLayout;
    m_settingsNavList = new QListWidget(m_settingsPage);
    m_settingsNavList->setFixedWidth(260);
    m_settingsNavList->addItems(QStringList{
        tr("Search"),
        tr("Remotes"),
        tr("Tracked Builds"),
        tr("Manage Leftover User Data"),
        tr("Keyboard Shortcuts"),
        tr("About")
    });
    settingsPanesLayout->addWidget(m_settingsNavList);

    m_settingsDetailStack = new QStackedWidget(m_settingsPage);

    auto *searchPage = new QWidget(m_settingsDetailStack);
    auto *searchPageLayout = new QVBoxLayout(searchPage);
    auto *searchTitle = new QLabel(tr("Search"), searchPage);
    QFont sectionTitleFont = searchTitle->font();
    sectionTitleFont.setBold(true);
    sectionTitleFont.setPointSizeF(sectionTitleFont.pointSizeF() + 1);
    searchTitle->setFont(sectionTitleFont);
    auto *delaySearchToggle = new QCheckBox(tr("Delay Search Results"), searchPage);
    delaySearchToggle->setChecked(m_delaySearchEnabled);
    auto *delaySearchHelp = new QLabel(tr("Improve results performance by debouncing search terms"), searchPage);
    delaySearchHelp->setWordWrap(true);
    delaySearchHelp->setStyleSheet(QStringLiteral("color: gray;"));
    searchPageLayout->addWidget(searchTitle);
    searchPageLayout->addWidget(delaySearchToggle);
    searchPageLayout->addWidget(delaySearchHelp);
    searchPageLayout->addStretch(1);
    m_settingsDetailStack->addWidget(searchPage);

    auto *remotesPage = new QWidget(m_settingsDetailStack);
    auto *remotesPageLayout = new QVBoxLayout(remotesPage);
    auto *remotesTitle = new QLabel(tr("Remotes"), remotesPage);
    remotesTitle->setFont(sectionTitleFont);
    auto *remotesHelp = new QLabel(tr("Add custom Flatpak remotes and view currently configured remotes."), remotesPage);
    remotesHelp->setWordWrap(true);
    remotesHelp->setStyleSheet(QStringLiteral("color: gray;"));
    m_remotesList = new QListWidget(remotesPage);
    m_remotesList->setMinimumHeight(120);
    m_remoteNameEdit = new QLineEdit(remotesPage);
    m_remoteNameEdit->setPlaceholderText(tr("Remote name (e.g. my-remote)"));
    m_remoteUrlEdit = new QLineEdit(remotesPage);
    m_remoteUrlEdit->setPlaceholderText(tr("Remote URL"));
    auto *remoteButtonsLayout = new QHBoxLayout;
    auto *refreshRemotesButton = new QPushButton(tr("Refresh"), remotesPage);
    auto *addRemoteButton = new QPushButton(tr("Add Remote"), remotesPage);
    m_removeRemoteButton = new QPushButton(tr("Remove Remote"), remotesPage);
    m_removeRemoteButton->setEnabled(false);
    remoteButtonsLayout->addWidget(refreshRemotesButton);
    remoteButtonsLayout->addWidget(addRemoteButton);
    remoteButtonsLayout->addWidget(m_removeRemoteButton);
    remoteButtonsLayout->addStretch(1);
    m_remoteStatusLabel = new QLabel(remotesPage);
    m_remoteStatusLabel->setWordWrap(true);
    m_remoteStatusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    remotesPageLayout->addWidget(remotesTitle);
    remotesPageLayout->addWidget(remotesHelp);
    remotesPageLayout->addWidget(m_remotesList);
    remotesPageLayout->addWidget(m_remoteNameEdit);
    remotesPageLayout->addWidget(m_remoteUrlEdit);
    remotesPageLayout->addLayout(remoteButtonsLayout);
    remotesPageLayout->addWidget(m_remoteStatusLabel);
    remotesPageLayout->addStretch(1);
    connect(refreshRemotesButton, &QPushButton::clicked, this, &MainWindow::onRefreshRemotesTriggered);
    connect(addRemoteButton, &QPushButton::clicked, this, &MainWindow::onAddRemoteTriggered);
    connect(m_removeRemoteButton, &QPushButton::clicked, this, &MainWindow::onRemoveRemoteTriggered);
    connect(m_remotesList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *) {
        if (!m_removeRemoteButton) {
            return;
        }
        if (!current) {
            m_removeRemoteButton->setEnabled(false);
            return;
        }
        const QString remoteName = current->data(Qt::UserRole).toString().trimmed();
        const bool removable = !remoteName.isEmpty()
                && remoteName.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) != 0;
        m_removeRemoteButton->setEnabled(removable);
    });
    m_settingsDetailStack->addWidget(remotesPage);

    auto *trackedPage = new QWidget(m_settingsDetailStack);
    auto *trackedPageLayout = new QVBoxLayout(trackedPage);
    auto *trackedTitle = new QLabel(tr("Tracked Builds"), trackedPage);
    trackedTitle->setFont(sectionTitleFont);
    auto *trackedHelp = new QLabel(
        tr("Track Git-hosted Flatpak release assets from GitHub, GitLab, or Codeberg."),
        trackedPage);
    trackedHelp->setWordWrap(true);
    trackedHelp->setStyleSheet(QStringLiteral("color: gray;"));
    m_trackedBuildsList = new QListWidget(trackedPage);
    m_trackedBuildsList->setMinimumHeight(160);
    auto *trackedButtonsLayout = new QHBoxLayout;
    m_trackedAddButton = new QPushButton(tr("Add Git Repo"), trackedPage);
    m_trackedEditButton = new QPushButton(tr("Edit"), trackedPage);
    m_trackedRemoveButton = new QPushButton(tr("Remove"), trackedPage);
    auto *trackedRefreshButton = new QPushButton(tr("Refresh Now"), trackedPage);
    m_trackedEditButton->setEnabled(false);
    m_trackedRemoveButton->setEnabled(false);
    trackedButtonsLayout->addWidget(m_trackedAddButton);
    trackedButtonsLayout->addWidget(m_trackedEditButton);
    trackedButtonsLayout->addWidget(m_trackedRemoveButton);
    trackedButtonsLayout->addWidget(trackedRefreshButton);
    trackedButtonsLayout->addStretch(1);
    m_trackedStatusLabel = new QLabel(trackedPage);
    m_trackedStatusLabel->setWordWrap(true);
    m_trackedStatusLabel->setStyleSheet(QStringLiteral("color: gray;"));

    auto *patTitle = new QLabel(
            tr("API tokens (recommended for GitHub — avoids rate limits; optional for private repos)"),
            trackedPage);
    patTitle->setFont(sectionTitleFont);
    m_githubPatEdit = new QLineEdit(trackedPage);
    m_githubPatEdit->setEchoMode(QLineEdit::Password);
    m_githubPatEdit->setPlaceholderText(tr("GitHub personal access token"));
    m_gitlabPatEdit = new QLineEdit(trackedPage);
    m_gitlabPatEdit->setEchoMode(QLineEdit::Password);
    m_gitlabPatEdit->setPlaceholderText(tr("GitLab personal access token"));
    m_codebergPatEdit = new QLineEdit(trackedPage);
    m_codebergPatEdit->setEchoMode(QLineEdit::Password);
    m_codebergPatEdit->setPlaceholderText(tr("Codeberg access token"));
    auto *savePatButton = new QPushButton(tr("Save Tokens"), trackedPage);
    CurioSettings &settings = CurioSettings::instance();
    settings.load();
    m_githubPatEdit->setText(settings.githubPat());
    m_gitlabPatEdit->setText(settings.gitlabPat());
    m_codebergPatEdit->setText(settings.codebergPat());

    trackedPageLayout->addWidget(trackedTitle);
    trackedPageLayout->addWidget(trackedHelp);
    trackedPageLayout->addWidget(m_trackedBuildsList);
    trackedPageLayout->addLayout(trackedButtonsLayout);
    trackedPageLayout->addWidget(m_trackedStatusLabel);
    trackedPageLayout->addWidget(patTitle);
    trackedPageLayout->addWidget(new QLabel(tr("GitHub token"), trackedPage));
    trackedPageLayout->addWidget(m_githubPatEdit);
    trackedPageLayout->addWidget(new QLabel(tr("GitLab token"), trackedPage));
    trackedPageLayout->addWidget(m_gitlabPatEdit);
    trackedPageLayout->addWidget(new QLabel(tr("Codeberg token"), trackedPage));
    trackedPageLayout->addWidget(m_codebergPatEdit);
    trackedPageLayout->addWidget(savePatButton);
    trackedPageLayout->addStretch(1);
    connect(m_trackedAddButton, &QPushButton::clicked, this, &MainWindow::onTrackedBuildAdd);
    connect(m_trackedEditButton, &QPushButton::clicked, this, &MainWindow::onTrackedBuildEdit);
    connect(m_trackedRemoveButton, &QPushButton::clicked, this, &MainWindow::onTrackedBuildRemove);
    connect(trackedRefreshButton, &QPushButton::clicked, this, &MainWindow::onTrackedBuildRefresh);
    connect(savePatButton, &QPushButton::clicked, this, [this]() {
        CurioSettings &s = CurioSettings::instance();
        s.setGithubPat(m_githubPatEdit->text());
        s.setGitlabPat(m_gitlabPatEdit->text());
        s.setCodebergPat(m_codebergPatEdit->text());
        if (m_trackedStatusLabel)
            m_trackedStatusLabel->setText(tr("API tokens saved."));
    });
    connect(m_trackedBuildsList, &QListWidget::currentRowChanged, this, [this]() {
        onTrackedBuildSelectionChanged();
    });
    m_settingsDetailStack->addWidget(trackedPage);

    auto *leftoverPage = new QWidget(m_settingsDetailStack);
    auto *leftoverPageLayout = new QVBoxLayout(leftoverPage);
    auto *leftoverTitle = new QLabel(tr("Manage Leftover User Data"), leftoverPage);
    leftoverTitle->setFont(sectionTitleFont);
    auto *leftoverHelp = new QLabel(tr("Review and clear cached/local app data files."), leftoverPage);
    leftoverHelp->setWordWrap(true);
    leftoverHelp->setStyleSheet(QStringLiteral("color: gray;"));
    m_leftoverDataPathLabel = new QLabel(leftoverPage);
    m_leftoverDataPathLabel->setWordWrap(true);
    m_leftoverDataPathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_leftoverDataList = new QListWidget(leftoverPage);
    m_leftoverDataList->setMinimumHeight(120);
    m_leftoverDataTotalLabel = new QLabel(leftoverPage);
    m_leftoverDataClearButton = new QPushButton(tr("Clear All"), leftoverPage);
    leftoverPageLayout->addWidget(leftoverTitle);
    leftoverPageLayout->addWidget(leftoverHelp);
    leftoverPageLayout->addWidget(m_leftoverDataPathLabel);
    leftoverPageLayout->addWidget(m_leftoverDataList);
    leftoverPageLayout->addWidget(m_leftoverDataTotalLabel);
    leftoverPageLayout->addWidget(m_leftoverDataClearButton, 0, Qt::AlignLeft);
    leftoverPageLayout->addStretch(1);
    connect(m_leftoverDataClearButton, &QPushButton::clicked, this, &MainWindow::onClearLeftoverUserDataTriggered);
    m_settingsDetailStack->addWidget(leftoverPage);

    auto *shortcutsPage = new QWidget(m_settingsDetailStack);
    auto *shortcutsPageLayout = new QVBoxLayout(shortcutsPage);
    auto *shortcutsTitle = new QLabel(tr("Keyboard Shortcuts"), shortcutsPage);
    shortcutsTitle->setFont(sectionTitleFont);
    auto *shortcutsHelp = new QLabel(tr("View available application shortcuts."), shortcutsPage);
    shortcutsHelp->setWordWrap(true);
    shortcutsHelp->setStyleSheet(QStringLiteral("color: gray;"));
    auto *shortcutsContent = new QLabel(
        tr("Open Search Page - Ctrl+F\n"
           "Open Library Page - Ctrl+D\n"
           "Refresh - Ctrl+R\n"
           "Open Preferences - Ctrl+,\n"
           "Show Shortcuts - Ctrl+?\n"
           "Close Window - Ctrl+W\n"
           "Quit Curio - Ctrl+Q"),
        shortcutsPage);
    shortcutsContent->setWordWrap(true);
    shortcutsPageLayout->addWidget(shortcutsTitle);
    shortcutsPageLayout->addWidget(shortcutsHelp);
    shortcutsPageLayout->addWidget(shortcutsContent);
    shortcutsPageLayout->addStretch(1);
    m_settingsDetailStack->addWidget(shortcutsPage);

    auto *aboutPage = new QWidget(m_settingsDetailStack);
    auto *aboutPageLayout = new QVBoxLayout(aboutPage);
    auto *aboutTitle = new QLabel(tr("About"), aboutPage);
    aboutTitle->setFont(sectionTitleFont);
    auto *aboutHelp = new QLabel(tr("Application version and project information."), aboutPage);
    aboutHelp->setWordWrap(true);
    aboutHelp->setStyleSheet(QStringLiteral("color: gray;"));
    auto *aboutContent = new QLabel(
        tr("Curio\n\nA GUI Flatpak store."),
        aboutPage);
    aboutContent->setWordWrap(true);
    aboutPageLayout->addWidget(aboutTitle);
    aboutPageLayout->addWidget(aboutHelp);
    aboutPageLayout->addWidget(aboutContent);
    aboutPageLayout->addStretch(1);
    m_settingsDetailStack->addWidget(aboutPage);

    settingsPanesLayout->addWidget(m_settingsDetailStack, 1);
    settingsLayout->addLayout(settingsPanesLayout, 1);

    auto *settingsScroll = new QScrollArea(this);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(m_settingsPage);
    settingsScroll->setMinimumHeight(120);
    m_stack->addWidget(settingsScroll);

    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &MainWindow::onSearchDebounceFired);
    m_bannerAutoScrollTimer = new QTimer(this);
    m_bannerAutoScrollTimer->setInterval(5000);
    connect(m_bannerAutoScrollTimer, &QTimer::timeout, this, [this]() {
        scrollBannerBy(1);
    });
    m_bannerAutoScrollTimer->start();

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_bannerPrevButton, &QPushButton::clicked, this, [this]() { scrollBannerBy(-1); });
    connect(m_bannerNextButton, &QPushButton::clicked, this, [this]() { scrollBannerBy(1); });
    connect(m_bannerOpenButton, &QPushButton::clicked, this, [this]() {
        if (m_bannerCurrentIndex >= 0 && m_bannerCurrentIndex < m_bannerApps.size())
            showDetailsForApp(m_bannerApps.at(m_bannerCurrentIndex).id);
    });
    connect(m_storeButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(0); });
    connect(m_searchButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(1); });
    connect(m_installedButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(2); });
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(3); });
    connect(m_storeTemplateTabs, &QTabBar::currentChanged, this, &MainWindow::onStoreTemplateChanged);
    connect(m_storeFeedTabs, &QTabBar::currentChanged, this, &MainWindow::onStoreFeedChanged);
    connect(m_settingsNavList, &QListWidget::currentRowChanged, this, &MainWindow::onSettingsSectionChanged);
    connect(m_checkUpdatesButton, &QPushButton::clicked, this, &MainWindow::onCheckForUpdatesTriggered);
    connect(delaySearchToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_delaySearchEnabled = checked;
    });

    auto openSettingsSection = [this](int sectionRow) {
        onTopTabChanged(3);
        if (m_settingsNavList)
            m_settingsNavList->setCurrentRow(sectionRow);
    };

    auto *openSearchShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this);
    openSearchShortcut->setContext(Qt::ApplicationShortcut);
    connect(openSearchShortcut, &QShortcut::activated, this, [this]() {
        onTopTabChanged(1);
        if (m_searchEdit)
            m_searchEdit->setFocus();
    });

    auto *openLibraryShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this);
    openLibraryShortcut->setContext(Qt::ApplicationShortcut);
    connect(openLibraryShortcut, &QShortcut::activated, this, [this]() {
        onTopTabChanged(2);
    });

    auto *refreshShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+R")), this);
    refreshShortcut->setContext(Qt::ApplicationShortcut);
    connect(refreshShortcut, &QShortcut::activated, this, [this]() {
        refreshAllData();
    });

    auto *openPreferencesShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
    openPreferencesShortcut->setContext(Qt::ApplicationShortcut);
    connect(openPreferencesShortcut, &QShortcut::activated, this, [openSettingsSection]() {
        openSettingsSection(0);
    });

    auto *showShortcutsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+?")), this);
    showShortcutsShortcut->setContext(Qt::ApplicationShortcut);
    connect(showShortcutsShortcut, &QShortcut::activated, this, [openSettingsSection]() {
        openSettingsSection(4);
    });
    auto *showShortcutsAltShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+/")), this);
    showShortcutsAltShortcut->setContext(Qt::ApplicationShortcut);
    connect(showShortcutsAltShortcut, &QShortcut::activated, this, [openSettingsSection]() {
        openSettingsSection(4);
    });

    auto *closeWindowShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+W")), this);
    closeWindowShortcut->setContext(Qt::ApplicationShortcut);
    connect(closeWindowShortcut, &QShortcut::activated, this, [this]() {
        close();
    });

    auto *quitAppShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Q")), this);
    quitAppShortcut->setContext(Qt::ApplicationShortcut);
    connect(quitAppShortcut, &QShortcut::activated, this, []() {
        qApp->quit();
    });

    m_settingsNavList->setCurrentRow(0);

    setWindowTitle(tr("Curio"));
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    setMinimumSize(360, 420);
    resize(900, 600);
}

void MainWindow::connectBackend()
{
    connect(m_backend, &FlatpakBackend::installedAppsUpdated,
            this, [this](const QVector<AppInfo> &apps) {
                m_installedModel->setApps(apps);
                rebuildInstalledRows();
                // Refresh details page if we're showing an app whose installed state may have changed
                if (m_stack->currentIndex() == 1 && !m_currentDetailsAppId.isEmpty()) {
                    AppInfo info;
                    for (const AppInfo &app : apps) {
                        if (app.id == m_currentDetailsAppId) {
                            info = app;
                            break;
                        }
                    }
                    if (info.id.isEmpty()) {
                        for (int i = 0; i < m_exploreModel->rowCount(); ++i) {
                            if (m_exploreModel->appAt(i).id == m_currentDetailsAppId) {
                                info = m_exploreModel->appAt(i);
                                break;
                            }
                        }
                    }
                    if (!info.id.isEmpty())
                        m_detailsWidget->setApp(info);
                }
            });

    connect(m_backend, &FlatpakBackend::installedAppsMetadataEnriched,
            this, [this](const QVector<AppInfo> &patches) {
                patchInstalledAppsMetadata(patches);
            });

    connect(m_backend, &FlatpakBackend::searchResultsUpdated,
            this, [this](const QVector<AppInfo> &apps) {
                m_exploreModel->setApps(apps);
                scheduleExploreRebuild();
            });

    connect(m_backend, &FlatpakBackend::storeCollectionsUpdated,
            this, [this](const QString &repoId,
                         const QVector<AppInfo> &trending,
                         const QVector<AppInfo> &popular,
                         const QVector<AppInfo> &recent,
                         const QVector<AppInfo> &updated) {
                if (repoId != m_activeStoreTemplate.repoId)
                    return;
                m_storeTrending = trending;
                m_storePopular = popular;
                m_storeRecent = recent;
                m_storeUpdated = updated;
                m_storeSuggestions = trending;
                setStoreFeedLoading(false);
                applyStoreDiscoveryData();
            });
    connect(m_backend, &FlatpakBackend::storeIconsEnriched,
            this, [this](const QString &repoId, const QVector<AppInfo> &apps) {
                if (repoId != m_activeStoreTemplate.repoId)
                    return;
                patchStoreCollectionsIcons(apps);
                m_exploreModel->patchApps(apps);
                refreshExploreCardIcons();
                if (m_topSectionIndex == 0 && m_bannerCurrentIndex >= 0
                    && m_bannerCurrentIndex < m_bannerApps.size()) {
                    for (const AppInfo &app : apps) {
                        if (m_bannerApps.at(m_bannerCurrentIndex).id == app.id) {
                            m_bannerApps[m_bannerCurrentIndex].iconName = app.iconName;
                            m_bannerApps[m_bannerCurrentIndex].iconUrl = app.iconUrl;
                            refreshActiveBannerCard();
                            break;
                        }
                    }
                }
            });
    connect(m_backend, &FlatpakBackend::storeFetchFailed,
            this, [this](const QString &repoId, const QString &message) {
                if (repoId != m_activeStoreTemplate.repoId)
                    return;
                setStoreFeedLoading(false);
                if (!m_storeStatusLabel)
                    return;
                const bool networkLike = message.contains(QStringLiteral("Network"), Qt::CaseInsensitive)
                        || message.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)
                        || message.contains(QStringLiteral("TLS"), Qt::CaseInsensitive)
                        || message.contains(QStringLiteral("Connection"), Qt::CaseInsensitive)
                        || message.contains(QStringLiteral("Host"), Qt::CaseInsensitive)
                        || message.contains(QStringLiteral("timed out"), Qt::CaseInsensitive);
                const QString guidance = networkLike
                        ? tr("Check your internet connection and try again.")
                        : tr("Open Settings (Ctrl+,) and go to Remotes to add or fix '%1'.")
                                  .arg(repoId);
                m_storeStatusLabel->setText(QStringLiteral("%1\n%2").arg(message, guidance));
                m_storeStatusLabel->setVisible(true);
                const bool hasCachedFeed = !m_storeTrending.isEmpty() || !m_storePopular.isEmpty()
                        || !m_storeRecent.isEmpty() || !m_storeUpdated.isEmpty();
                if (m_topSectionIndex == 0 && !hasCachedFeed) {
                    m_exploreModel->setApps({});
                    scheduleExploreRebuild();
                    refreshStoreCategoriesPane();
                }
            });
    connect(m_backend, &FlatpakBackend::storeCatalogIndexReady,
            this, [this](const QString &repoId, const QVector<AppInfo> &patches) {
                if (repoId != m_activeStoreTemplate.repoId || patches.isEmpty())
                    return;
                const auto patchList = [&patches](QVector<AppInfo> &list) {
                    for (AppInfo &app : list) {
                        for (const AppInfo &update : patches) {
                            if (app.id == update.id) {
                                if (app.name.isEmpty() || app.name == app.id)
                                    app.name = update.name;
                                if (app.summary.isEmpty())
                                    app.summary = update.summary;
                                if (app.version.isEmpty())
                                    app.version = update.version;
                                break;
                            }
                        }
                    }
                };
                patchList(m_storeTrending);
                patchList(m_storePopular);
                patchList(m_storeRecent);
                patchList(m_storeUpdated);
                patchList(m_storeSuggestions);
                patchStoreCollectionsIcons(patches);
                m_exploreModel->patchApps(patches);
                refreshExploreCardIcons();
                if (m_topSectionIndex == 0)
                    scheduleExploreRebuild();
            });

    connect(m_backend, &FlatpakBackend::operationStarted,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
                updateInstalledRowOperation(op, false);
                updateTrackedInstallFeedback(op, false);
            });
    connect(m_backend, &FlatpakBackend::operationProgress,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
                updateInstalledRowOperation(op, false);
                updateTrackedInstallFeedback(op, false);
            });
    connect(m_backend, &FlatpakBackend::operationFinished,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
                updateInstalledRowOperation(op, true);
                updateTrackedInstallFeedback(op, true);
                m_backend->refreshInstalled();
            });

    connect(m_backend, &FlatpakBackend::remotesUpdated,
            this, [this](const QVector<QPair<QString, QString>> &remotes) {
                if (!m_remotesList || !m_remoteStatusLabel)
                    return;
                m_remotesList->clear();
                for (const auto &remote : remotes) {
                    const QString line = remote.second.isEmpty()
                            ? remote.first
                            : QStringLiteral("%1  -  %2").arg(remote.first, remote.second);
                    auto *item = new QListWidgetItem(line, m_remotesList);
                    item->setData(Qt::UserRole, remote.first);
                    if (remote.first.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
                        item->setToolTip(tr("Protected remote"));
                    }
                }
                if (remotes.isEmpty()) {
                    auto *item = new QListWidgetItem(tr("No remotes found."), m_remotesList);
                    item->setData(Qt::UserRole, QString());
                }
                if (m_removeRemoteButton) {
                    QListWidgetItem *current = m_remotesList->currentItem();
                    const QString currentName = current ? current->data(Qt::UserRole).toString().trimmed()
                                                        : QString();
                    const bool removable = !currentName.isEmpty()
                            && currentName.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) != 0;
                    m_removeRemoteButton->setEnabled(removable);
                }
                m_remoteStatusLabel->setText(tr("Found %1 remote(s).").arg(remotes.size()));
                updateStoreTemplatesForRemotes(remotes);
            });

    connect(m_backend, &FlatpakBackend::remoteAddFinished,
            this, [this](bool ok, const QString &message) {
                if (!m_remoteStatusLabel)
                    return;
                m_remoteStatusLabel->setText(message);
                if (ok) {
                    if (m_remoteNameEdit)
                        m_remoteNameEdit->clear();
                    if (m_remoteUrlEdit)
                        m_remoteUrlEdit->clear();
                }
            });

    connect(m_backend, &FlatpakBackend::remoteRemoveFinished,
            this, [this](bool, const QString &message) {
                if (!m_remoteStatusLabel)
                    return;
                m_remoteStatusLabel->setText(message);
            });

    connect(m_backend, &FlatpakBackend::trackedBuildProjectsUpdated,
            this, [this](const QVector<TrackedBuildProject> &projects) {
                m_trackedBuildProjects = projects;
                refreshTrackedBuildsList();
            });
    connect(m_backend, &FlatpakBackend::trackedBuildRefreshFailed,
            this, [this](const QString &message) {
                if (m_trackedStatusLabel)
                    m_trackedStatusLabel->setText(message);
            });
    connect(m_backend, &FlatpakBackend::trackedBuildRefreshFinished,
            this, [this]() {
                if (m_trackedStatusLabel)
                    m_trackedStatusLabel->setText(tr("Tracked build refresh complete."));
                m_trackedBuildProjects = m_backend->trackedBuildProjects();
                patchInstalledTrackedBuildMetadata();
                const bool userCheck = m_checkForUpdatesPending;
                QTimer::singleShot(0, this, [this, userCheck]() {
                    refreshTrackedUpdateBanner();
                    finishCheckForUpdatesFeedback(userCheck);
                });
                m_backend->refreshInstalled();
            });

    m_trackedBuildProjects = m_backend->trackedBuildProjects();
}

void MainWindow::clearExploreGridLayout()
{
    auto *grid = qobject_cast<QGridLayout *>(m_exploreContainer ? m_exploreContainer->layout() : nullptr);
    if (!grid)
        return;
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}

void MainWindow::rebuildExploreCards()
{
    auto *grid = qobject_cast<QGridLayout *>(m_exploreContainer->layout());
    if (!grid)
        return;

    QVector<AppInfo> apps;
    apps.reserve(m_exploreModel->rowCount());
    for (int i = 0; i < m_exploreModel->rowCount(); ++i)
        apps.append(m_exploreModel->appAt(i));

    const QString signature = exploreModelSignature(apps);
    const bool sameApps = (signature == m_lastExploreModelSignature);

    const int spacing = grid->horizontalSpacing() > 0 ? grid->horizontalSpacing() : 12;
    const int availableWidth = exploreAvailableWidth(m_exploreContainer, grid);
    const int columns = exploreColumnCount(availableWidth, spacing);

    QHash<QString, AppCardWidget *> existingCards;
    for (int i = 0; i < grid->count(); ++i) {
        if (auto *card = qobject_cast<AppCardWidget *>(grid->itemAt(i)->widget()))
            existingCards.insert(card->appId(), card);
    }

    if (sameApps && !apps.isEmpty() && columns == m_lastExploreColumnCount) {
        for (const AppInfo &app : apps) {
            if (AppCardWidget *card = existingCards.value(app.id))
                card->setApp(app);
        }
        return;
    }

    const bool reflowOnly = sameApps && !apps.isEmpty();
    if (!reflowOnly)
        m_lastExploreModelSignature = signature;
    m_lastExploreColumnCount = columns;
    m_exploreContainer->setUpdatesEnabled(false);

    if (reflowOnly) {
        while (QLayoutItem *item = grid->takeAt(0)) {
            if (item->widget())
                item->widget()->setParent(m_exploreContainer);
            delete item;
        }
    } else {
        clearExploreGridLayout();
    }

    const int count = apps.size();

    if (count == 0) {
        for (AppCardWidget *card : std::as_const(existingCards))
            card->deleteLater();
        const QString emptyText = m_storeFeedLoading
                ? tr("Refreshing catalog…")
                : tr("No results found.");
        auto *emptyLabel = new QLabel(emptyText, m_exploreContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: gray; padding: 24px;"));
        grid->addWidget(emptyLabel, 0, 0, 1, columns);
        m_exploreContainer->setUpdatesEnabled(true);
        return;
    }

    QSet<QString> usedIds;
    for (int i = 0; i < count; ++i) {
        const AppInfo &app = apps.at(i);
        usedIds.insert(app.id);

        AppCardWidget *card = existingCards.take(app.id);
        if (!card) {
            card = new AppCardWidget(m_exploreContainer);
            connect(card, &AppCardWidget::openDetailsRequested, this, [this, app]() {
                showDetailsForApp(app.id);
            });
            connect(card, &AppCardWidget::installRequested, this, [this, app]() {
                onExploreInstallRequested(app.id);
            });
        }
        card->setApp(app);
        const int row = i / columns;
        const int col = i % columns;
        grid->addWidget(card, row, col, Qt::AlignTop | Qt::AlignHCenter);
    }

    for (int col = 0; col < columns; ++col)
        grid->setColumnStretch(col, 0);
    for (int row = 0; row < grid->rowCount(); ++row)
        grid->setRowStretch(row, 0);
    grid->setRowStretch(grid->rowCount(), 1);

    for (auto it = existingCards.constBegin(); it != existingCards.constEnd(); ++it) {
        if (!usedIds.contains(it.key()))
            it.value()->deleteLater();
    }

    m_exploreContainer->setUpdatesEnabled(true);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        scheduleExploreRebuild();
    });
    if (!m_deferredStartupDone) {
        m_deferredStartupDone = true;
        QTimer::singleShot(0, this, &MainWindow::runDeferredStartup);
    }
}

void MainWindow::runDeferredStartup()
{
    m_backend->refreshInstalled();
    QTimer::singleShot(250, this, [this]() {
        m_backend->listRemotes();
    });
    QTimer::singleShot(150, this, [this]() {
        beginStoreRefresh();
    });
    QTimer::singleShot(2000, this, [this]() {
        m_backend->refreshTrackedBuilds();
    });
}

void MainWindow::applyStoreDiscoveryData()
{
    if (m_storeStatusLabel)
        m_storeStatusLabel->setVisible(false);

    QVector<AppInfo> bannerPool;
    bannerPool.reserve(m_storeTrending.size() + m_storePopular.size()
                       + m_storeRecent.size() + m_storeUpdated.size());
    bannerPool += m_storeTrending;
    bannerPool += m_storePopular;
    bannerPool += m_storeRecent;
    bannerPool += m_storeUpdated;
    if (bannerPool.isEmpty())
        bannerPool = m_storeSuggestions;
    refreshFeaturedBanner(bannerPool);

    if (m_topSectionIndex == 0)
        onStoreFeedChanged(m_storeFeedTabs->currentIndex());

    if (m_activeStoreTemplate.supportsCategories && m_storeFeedTabs
        && m_storeFeedTabs->currentIndex() == 4) {
        QTimer::singleShot(0, this, [this]() {
            refreshStoreCategoriesPane();
        });
    }
}

void MainWindow::scheduleExploreRebuild()
{
    if (m_exploreRebuildScheduled)
        return;
    m_exploreRebuildScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_exploreRebuildScheduled = false;
        rebuildExploreCards();
    });
}

void MainWindow::patchStoreCollectionsIcons(const QVector<AppInfo> &updates)
{
    const auto patchList = [&updates](QVector<AppInfo> &list) {
        for (AppInfo &app : list) {
            for (const AppInfo &update : updates) {
                if (app.id == update.id) {
                    app.iconName = update.iconName;
                    app.iconUrl = update.iconUrl;
                    break;
                }
            }
        }
    };
    patchList(m_storeTrending);
    patchList(m_storePopular);
    patchList(m_storeRecent);
    patchList(m_storeUpdated);
    patchList(m_storeSuggestions);
    patchList(m_storeCategoryPool);
    patchList(m_bannerApps);
}

void MainWindow::refreshExploreCardIcons()
{
    auto *grid = qobject_cast<QGridLayout *>(m_exploreContainer ? m_exploreContainer->layout() : nullptr);
    if (!grid)
        return;

    QHash<QString, AppInfo> byId;
    const auto addApps = [&byId](const QVector<AppInfo> &list) {
        for (const AppInfo &app : list) {
            if (!app.id.isEmpty())
                byId.insert(app.id, app);
        }
    };
    for (int row = 0; row < m_exploreModel->rowCount(); ++row)
        addApps({m_exploreModel->appAt(row)});
    addApps(m_storeTrending);
    addApps(m_storePopular);
    addApps(m_storeRecent);
    addApps(m_storeUpdated);
    addApps(m_storeCategoryPool);

    for (int i = 0; i < grid->count(); ++i) {
        auto *card = qobject_cast<AppCardWidget *>(grid->itemAt(i)->widget());
        if (!card)
            continue;
        const auto it = byId.constFind(card->appId());
        if (it != byId.cend())
            card->patchIcon(it.value());
    }

    if (m_storeCategoryCardsContainer) {
        auto *categoryGrid = qobject_cast<QGridLayout *>(m_storeCategoryCardsContainer->layout());
        if (categoryGrid) {
            for (int i = 0; i < categoryGrid->count(); ++i) {
                auto *card = qobject_cast<AppCardWidget *>(categoryGrid->itemAt(i)->widget());
                if (!card)
                    continue;
                const auto it = byId.constFind(card->appId());
                if (it != byId.cend())
                    card->patchIcon(it.value());
            }
        }
    }
}

void MainWindow::rebuildInstalledRows()
{
    auto *vbox = qobject_cast<QVBoxLayout *>(m_installedContainer->layout());
    if (!vbox)
        return;

    m_installedContainer->setUpdatesEnabled(false);
    m_installedRowsByAppId.clear();

    // Keep check-for-updates header rows pinned (button + status label at indices 0–1).
    while (vbox->count() > 2) {
        QLayoutItem *item = vbox->takeAt(2);
        if (!item)
            break;
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    const int count = m_installedModel->rowCount();
    for (int i = 0; i < count; ++i) {
        AppInfo app = m_installedModel->appAt(i);
        auto *row = new InstalledRowWidget(m_installedContainer);
        row->setApp(app);
        configureInstalledRowSource(row, app);
        vbox->addWidget(row);
        if (!app.id.isEmpty())
            m_installedRowsByAppId.insert(app.id, row);

        connect(row, &InstalledRowWidget::openDetailsRequested, this, [this, app]() {
            showDetailsForApp(app.id);
        });
        connect(row, &InstalledRowWidget::uninstallRequested, this, [this, app]() {
            onInstalledUninstallRequested(app.id);
        });
        connect(row, &InstalledRowWidget::updateRequested, this, [this, app]() {
            m_backend->update(app.id, app.version);
        });
        connect(row, &InstalledRowWidget::trackGitRepoRequested, this, [this, app]() {
            openTrackedBuildDialogForApp(app);
        });
    }

    m_installedContainer->setUpdatesEnabled(true);
}

void MainWindow::updateInstalledRowOperation(const Operation &op, bool finished)
{
    InstalledRowWidget *row = m_installedRowsByAppId.value(op.appId);
    if (!row)
        return;

    if (op.type == OperationType::Uninstall) {
        row->setUninstallInProgress(!finished && op.status == OperationStatus::Running);
        return;
    }

    if (op.type == OperationType::Install || op.type == OperationType::Update) {
        if (!finished && op.status == OperationStatus::Running)
            row->setUpdateInProgress(true, op.message, op.progress);
        else
            row->setUpdateInProgress(false);
    }
}

void MainWindow::beginTrackedInstallFeedback(const QString &appId, const QString &releaseTag)
{
    const QString trimmedId = appId.trimmed();
    if (trimmedId.isEmpty())
        return;

    m_trackedInstallFeedbackAppId = trimmedId;

    if (m_stack && m_stack->currentIndex() != 0)
        m_stack->setCurrentIndex(0);
    onTopTabChanged(2);

    if (!m_checkUpdatesStatusLabel)
        return;

    const QString versionLabel = releaseTag.trimmed().isEmpty() ? QString() : releaseTag.trimmed();
    m_checkUpdatesStatusLabel->setStyleSheet(
            QStringLiteral("color: rgba(120, 180, 255, 0.95); font-weight: 500;"));
    m_checkUpdatesStatusLabel->setText(
            versionLabel.isEmpty()
                    ? tr("Installing %1…").arg(trimmedId)
                    : tr("Installing %1 (%2)…").arg(trimmedId, versionLabel));
    m_checkUpdatesStatusLabel->show();
}

void MainWindow::updateTrackedInstallFeedback(const Operation &op, bool finished)
{
    if (m_trackedInstallFeedbackAppId.isEmpty() || op.appId != m_trackedInstallFeedbackAppId)
        return;
    if (!m_checkUpdatesStatusLabel)
        return;

    const QString activeStyle = QStringLiteral("color: rgba(120, 180, 255, 0.95); font-weight: 500;");
    const QString successStyle = QStringLiteral("color: rgba(100, 200, 140, 0.95); font-weight: 500;");
    const QString errorStyle = QStringLiteral("color: rgba(232, 120, 120, 0.95); font-weight: 500;");

    if (finished) {
        if (op.status == OperationStatus::Succeeded) {
            m_checkUpdatesStatusLabel->setStyleSheet(successStyle);
            m_checkUpdatesStatusLabel->setText(
                    op.message.isEmpty()
                            ? tr("Installed %1 successfully.").arg(op.appId)
                            : op.message);
        } else {
            m_checkUpdatesStatusLabel->setStyleSheet(errorStyle);
            m_checkUpdatesStatusLabel->setText(
                    op.message.isEmpty() ? tr("Install failed for %1.").arg(op.appId) : op.message);
        }
        m_trackedInstallFeedbackAppId.clear();
    } else {
        m_checkUpdatesStatusLabel->setStyleSheet(activeStyle);
        m_checkUpdatesStatusLabel->setText(op.message.isEmpty() ? tr("Installing…") : op.message);
    }
    m_checkUpdatesStatusLabel->show();
}

bool MainWindow::presentTrackedBuildSetupDialog(TrackedBuildSetupDialog::Mode mode,
                                              const TrackedBuildProject &existing,
                                              const TrackedBuildSetupHints &hints)
{
    TrackedBuildSetupDialog dialog(m_backend, mode, existing, installedAppsSnapshot(), hints, this);
    connect(&dialog,
            &TrackedBuildSetupDialog::installRequested,
            this,
            &MainWindow::onTrackedBuildInstallRequested);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    m_backend->upsertTrackedBuildProject(dialog.resultProject());
    return true;
}

void MainWindow::onTrackedBuildInstallRequested(const TrackedBuildProject &project,
                                                const QString &assetUrl,
                                                const QString &linkedAppId,
                                                const QString &releaseTag)
{
    const QString appId = linkedAppId.trimmed().isEmpty() ? project.linkedAppId.trimmed() : linkedAppId.trimmed();
    m_backend->upsertTrackedBuildProject(project);
    beginTrackedInstallFeedback(appId, releaseTag);
    if (!m_backend->startTrackedBuildInstall(project, assetUrl, appId, releaseTag)
            && m_checkUpdatesStatusLabel) {
        m_checkUpdatesStatusLabel->setStyleSheet(
                QStringLiteral("color: rgba(232, 120, 120, 0.95); font-weight: 500;"));
        m_checkUpdatesStatusLabel->setText(
                tr("Could not start install for %1. Wait for any in-progress install to finish, then try again.")
                        .arg(appId));
        m_checkUpdatesStatusLabel->show();
        m_trackedInstallFeedbackAppId.clear();
    }
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    if (m_topSectionIndex != 1)
        return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_searchDebounceTimer->stop();
        m_lastSearchQuery.clear();
        m_exploreModel->setApps({});
        scheduleExploreRebuild();
        return;
    }
    if (!m_delaySearchEnabled) {
        if (trimmed != m_lastSearchQuery) {
            m_lastSearchQuery = trimmed;
            m_backend->search(trimmed);
        }
        return;
    }
    m_searchDebounceTimer->start(m_searchDebounceMs);
}

void MainWindow::onSearchDebounceFired()
{
    if (m_topSectionIndex != 1)
        return;

    const QString query = m_searchEdit->text().trimmed();
    if (!query.isEmpty() && query != m_lastSearchQuery) {
        m_lastSearchQuery = query;
        m_backend->search(query);
    }
}

void MainWindow::onTopTabChanged(int index)
{
    m_topSectionIndex = index;
    updateGeometry();
    if (m_storeButton && m_searchButton && m_installedButton && m_settingsButton) {
        if (index == 0)
            m_storeButton->setChecked(true);
        else if (index == 1)
            m_searchButton->setChecked(true);
        else if (index == 2)
            m_installedButton->setChecked(true);
        else if (index == 3)
            m_settingsButton->setChecked(true);
    }

    if (index == 3) {
        if (m_bannerWidget)
            m_bannerWidget->setVisible(false);
        m_searchEdit->setVisible(false);
        m_storeTemplateTabs->setVisible(false);
        m_storeFeedTabs->setVisible(false);
        m_stack->setCurrentIndex(2);
        updateInstalledNavAttention();
        return;
    }

    if (m_bannerWidget)
        m_bannerWidget->setVisible(index == 0);
    m_searchEdit->setVisible(index == 1);
    m_storeTemplateTabs->setVisible(index == 0);
    m_storeFeedTabs->setVisible(index == 0);
    if (m_storeStatusLabel)
        m_storeStatusLabel->setVisible(index == 0 && !m_storeStatusLabel->text().trimmed().isEmpty());
    m_stack->setCurrentIndex(0);

    if (index == 2) {
        m_mainContent->setCurrentIndex(2); // Installed
        updateInstalledNavAttention();
        return;
    }

    if (index == 1) {
        m_mainContent->setCurrentIndex(0); // Explore for search results
        if (!m_searchEdit->text().trimmed().isEmpty())
            onSearchDebounceFired();
        else {
            m_exploreModel->setApps({});
            scheduleExploreRebuild();
        }
        updateInstalledNavAttention();
        return;
    }

    // Default store tab
    onStoreFeedChanged(m_storeFeedTabs->currentIndex());
    updateInstalledNavAttention();
}

void MainWindow::onStoreTemplateChanged(int index)
{
    if (index < 0 || index >= m_storeTemplates.size())
        return;
    m_activeStoreTemplate = m_storeTemplates.at(index);
    applyActiveStoreTemplateUi();
    if (m_storeStatusLabel) {
        m_storeStatusLabel->clear();
        m_storeStatusLabel->setVisible(false);
    }
    beginStoreRefresh();
}

void MainWindow::onStoreFeedChanged(int index)
{
    if (m_topSectionIndex != 0)
        return;

    if (m_activeStoreTemplate.supportsCategories && index == 4) {
        m_mainContent->setCurrentIndex(1); // Store categories
        refreshStoreCategoriesPane();
        return;
    }

    m_mainContent->setCurrentIndex(0); // Explore feed cards
    switch (index) {
    case 1:
        m_exploreModel->setApps(m_storePopular);
        break;
    case 2:
        m_exploreModel->setApps(m_storeRecent);
        break;
    case 3:
        m_exploreModel->setApps(m_storeUpdated);
        break;
    case 0:
    default:
        m_exploreModel->setApps(m_storeTrending.isEmpty() ? m_storeSuggestions : m_storeTrending);
        break;
    }
    scheduleExploreRebuild();
}

void MainWindow::onSettingsSectionChanged(int row)
{
    if (!m_settingsDetailStack)
        return;
    if (row < 0 || row >= m_settingsDetailStack->count())
        return;
    m_settingsDetailStack->setCurrentIndex(row);
    if (row == 1)
        onRefreshRemotesTriggered();
    if (row == 2) {
        m_trackedBuildProjects = m_backend->trackedBuildProjects();
        refreshTrackedBuildsList();
    }
    if (row == 3)
        refreshLeftoverDataPane();
}

void MainWindow::showDetailsForApp(const QString &appId)
{
    m_currentDetailsAppId = appId;
    AppInfo info;
    for (int i = 0; i < m_exploreModel->rowCount(); ++i) {
        if (m_exploreModel->appAt(i).id == appId) {
            info = m_exploreModel->appAt(i);
            break;
        }
    }
    if (!info.id.isEmpty()) {
        prepareDetailsAppInfo(info);
        m_searchEdit->setVisible(false);
        m_storeTemplateTabs->setVisible(false);
        m_storeFeedTabs->setVisible(false);
        m_detailsWidget->setApp(info);
        m_stack->setCurrentIndex(1);
        return;
    }
    for (int i = 0; i < m_installedModel->rowCount(); ++i) {
        if (m_installedModel->appAt(i).id == appId) {
            info = m_installedModel->appAt(i);
            break;
        }
    }
    if (!info.id.isEmpty()) {
        prepareDetailsAppInfo(info);
        m_searchEdit->setVisible(false);
        m_storeTemplateTabs->setVisible(false);
        m_storeFeedTabs->setVisible(false);
        m_detailsWidget->setApp(info);
        m_stack->setCurrentIndex(1);
    }
}

void MainWindow::onExploreInstallRequested(const QString &appId)
{
    QString repoId = m_activeStoreTemplate.repoId;
    for (int i = 0; i < m_exploreModel->rowCount(); ++i) {
        const AppInfo app = m_exploreModel->appAt(i);
        if (app.id == appId) {
            if (!app.repoId.isEmpty())
                repoId = app.repoId;
            break;
        }
    }
    m_backend->install(appId, repoId);
    showDetailsForApp(appId);
}

void MainWindow::onInstalledUninstallRequested(const QString &appId)
{
    if (InstalledRowWidget *row = m_installedRowsByAppId.value(appId))
        row->setUninstallInProgress(true);
    m_backend->uninstall(appId);
}

void MainWindow::onCheckForUpdatesTriggered()
{
    m_checkForUpdatesPending = true;
    if (m_checkUpdatesButton) {
        m_checkUpdatesButton->setEnabled(false);
        m_checkUpdatesButton->setText(tr("Checking…"));
    }
    if (m_checkUpdatesStatusLabel) {
        m_checkUpdatesStatusLabel->setText(
                tr("Checking tracked build feeds (GitHub / GitLab / Codeberg)…"));
        m_checkUpdatesStatusLabel->show();
    }
    m_backend->refreshTrackedBuilds();
}

void MainWindow::onClearLeftoverUserDataTriggered()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo &fi : files)
        QFile::remove(fi.absoluteFilePath());
    refreshLeftoverDataPane();
}

void MainWindow::onRefreshRemotesTriggered()
{
    if (m_remotesList)
        m_remotesList->clear();
    if (m_removeRemoteButton)
        m_removeRemoteButton->setEnabled(false);
    if (m_remoteStatusLabel)
        m_remoteStatusLabel->setText(tr("Loading remotes..."));
    m_backend->listRemotes();
}

void MainWindow::onAddRemoteTriggered()
{
    if (!m_remoteNameEdit || !m_remoteUrlEdit || !m_remoteStatusLabel)
        return;

    const QString name = m_remoteNameEdit->text().trimmed();
    const QString url = m_remoteUrlEdit->text().trimmed();
    if (name.isEmpty() || url.isEmpty()) {
        m_remoteStatusLabel->setText(tr("Remote name and URL are required."));
        return;
    }

    m_remoteStatusLabel->setText(tr("Adding remote..."));
    m_backend->addRemote(name, url);
}

void MainWindow::onRemoveRemoteTriggered()
{
    if (!m_remotesList || !m_remoteStatusLabel)
        return;
    QListWidgetItem *item = m_remotesList->currentItem();
    if (!item) {
        m_remoteStatusLabel->setText(tr("Select a remote to remove."));
        return;
    }
    const QString remoteName = item->data(Qt::UserRole).toString().trimmed();
    if (remoteName.isEmpty()) {
        m_remoteStatusLabel->setText(tr("Select a valid remote to remove."));
        return;
    }
    if (remoteName.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        m_remoteStatusLabel->setText(tr("Flathub cannot be removed."));
        return;
    }

    m_remoteStatusLabel->setText(tr("Removing remote..."));
    m_backend->removeRemote(remoteName);
}

void MainWindow::refreshLeftoverDataPane()
{
    if (!m_leftoverDataPathLabel || !m_leftoverDataList || !m_leftoverDataTotalLabel || !m_leftoverDataClearButton)
        return;

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

    m_leftoverDataPathLabel->setText(tr("Cache and local data files in:\n%1").arg(dataDir));
    m_leftoverDataList->clear();

    qint64 totalSize = 0;
    for (const QFileInfo &fi : files) {
        totalSize += fi.size();
        m_leftoverDataList->addItem(QStringLiteral("%1 (%2 KB)")
                                        .arg(fi.fileName())
                                        .arg(QString::number(fi.size() / 1024.0, 'f', 1)));
    }
    if (files.isEmpty())
        m_leftoverDataList->addItem(tr("No leftover user data files found."));

    m_leftoverDataTotalLabel->setText(tr("Total size: %1 KB").arg(QString::number(totalSize / 1024.0, 'f', 1)));
    m_leftoverDataClearButton->setEnabled(!files.isEmpty());
}

void MainWindow::refreshAllData()
{
    m_backend->refreshInstalled();
    m_backend->refreshTrackedBuilds();
    beginStoreRefresh(true);
    const QString query = m_searchEdit->text().trimmed();
    if (m_topSectionIndex == 1 && !query.isEmpty())
        m_backend->search(query);
}

void MainWindow::refreshFeaturedBanner(const QVector<AppInfo> &apps)
{
    if (!m_bannerTitleLabel || !m_bannerSummaryLabel || !m_bannerIconLabel)
        return;

    QVector<AppInfo> unique;
    unique.reserve(apps.size());
    QSet<QString> seen;
    for (const AppInfo &app : apps) {
        if (app.id.isEmpty() || seen.contains(app.id))
            continue;
        seen.insert(app.id);
        unique.append(app);
        if (unique.size() >= 12)
            break;
    }
    m_bannerApps = unique;
    if (m_bannerCurrentIndex >= m_bannerApps.size())
        m_bannerCurrentIndex = 0;
    refreshActiveBannerCard();
}

void MainWindow::refreshActiveBannerCard()
{
    if (!m_bannerTitleLabel || !m_bannerSummaryLabel || !m_bannerIconLabel || !m_bannerOpenButton)
        return;

    if (m_bannerApps.isEmpty()) {
        m_bannerTitleLabel->setText(tr("Discover apps"));
        m_bannerSummaryLabel->setText(m_activeStoreTemplate.bannerEmptySummary);
        const QIcon icon = QIcon::fromTheme(QStringLiteral("applications-other"));
        m_bannerIconLabel->setPixmap(icon.pixmap(72, 72));
        m_bannerOpenButton->setEnabled(false);
        if (m_bannerPrevButton)
            m_bannerPrevButton->setEnabled(false);
        if (m_bannerNextButton)
            m_bannerNextButton->setEnabled(false);
        return;
    }

    if (m_bannerCurrentIndex < 0 || m_bannerCurrentIndex >= m_bannerApps.size())
        m_bannerCurrentIndex = 0;
    const AppInfo app = m_bannerApps.at(m_bannerCurrentIndex);
    m_bannerTitleLabel->setText(app.name.isEmpty() ? app.id : app.name);
    m_bannerSummaryLabel->setText(app.summary.isEmpty()
                                  ? m_activeStoreTemplate.featuredSummaryFallback
                                  : app.summary);
    QIcon icon;
    if (!app.iconName.isEmpty())
        icon = QIcon::fromTheme(app.iconName);
    if (icon.isNull() && !app.id.isEmpty())
        icon = QIcon::fromTheme(app.id);
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral("applications-other"));
    m_bannerIconLabel->setPixmap(icon.pixmap(72, 72));
    m_bannerOpenButton->setEnabled(!app.id.isEmpty());
    const bool multi = m_bannerApps.size() > 1;
    if (m_bannerPrevButton)
        m_bannerPrevButton->setEnabled(multi);
    if (m_bannerNextButton)
        m_bannerNextButton->setEnabled(multi);

    const QVector<QString> bgColors = {
        QStringLiteral("#2ea56b"),
        QStringLiteral("#6055d6"),
        QStringLiteral("#ba6d2b"),
        QStringLiteral("#22739d"),
        QStringLiteral("#8b4cad")
    };
    m_bannerWidget->setStyleSheet(QStringLiteral(
        "#featuredBanner { background-color: %1; border-radius: 12px; }"
    ).arg(bgColors.at(m_bannerCurrentIndex % bgColors.size())));
}

void MainWindow::scrollBannerBy(int direction)
{
    if (m_bannerApps.isEmpty())
        return;
    const int count = m_bannerApps.size();
    m_bannerCurrentIndex = (m_bannerCurrentIndex + (direction < 0 ? -1 : +1) + count) % count;
    refreshActiveBannerCard();
    if (m_bannerAutoScrollTimer)
        m_bannerAutoScrollTimer->start();
}

void MainWindow::refreshStoreCategoriesPane()
{
    if (!m_storeCategoriesList)
        return;

    QVector<AppInfo> pool;
    pool.reserve(m_storeTrending.size() + m_storePopular.size() + m_storeRecent.size() + m_storeUpdated.size());
    QSet<QString> seenIds;
    auto appendUnique = [&pool, &seenIds](const QVector<AppInfo> &src) {
        for (const AppInfo &app : src) {
            if (app.id.isEmpty() || seenIds.contains(app.id))
                continue;
            seenIds.insert(app.id);
            pool.append(app);
        }
    };
    appendUnique(m_storeTrending);
    appendUnique(m_storePopular);
    appendUnique(m_storeRecent);
    appendUnique(m_storeUpdated);
    if (pool.isEmpty())
        appendUnique(m_storeSuggestions);

    m_storeCategoryPool = pool;

    QSet<QString> categoriesSet;
    for (const AppInfo &app : m_storeCategoryPool) {
        for (const QString &cat : app.categories) {
            const QString trimmed = cat.trimmed();
            if (!trimmed.isEmpty())
                categoriesSet.insert(trimmed);
        }
    }

    QStringList categories = categoriesSet.values();
    categories.sort(Qt::CaseInsensitive);
    if (categories.isEmpty()) {
        categories = QStringList{
            QStringLiteral("Development"),
            QStringLiteral("Games"),
            QStringLiteral("Graphics"),
            QStringLiteral("AudioVideo"),
            QStringLiteral("Office"),
            QStringLiteral("Education"),
            QStringLiteral("Science"),
            QStringLiteral("Utility"),
            QStringLiteral("Network"),
            QStringLiteral("System")
        };
    }

    const QString prevSelection = m_selectedStoreCategory;
    m_storeCategoriesList->clear();
    for (int i = 0; i < categories.size(); ++i) {
        auto *item = new QListWidgetItem(categories.at(i), m_storeCategoriesList);
        item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    }

    int selectedRow = -1;
    if (!prevSelection.isEmpty()) {
        for (int i = 0; i < m_storeCategoriesList->count(); ++i) {
            if (m_storeCategoriesList->item(i)->text().compare(prevSelection, Qt::CaseInsensitive) == 0) {
                selectedRow = i;
                break;
            }
        }
    }
    if (selectedRow < 0 && m_storeCategoriesList->count() > 0)
        selectedRow = 0;
    if (selectedRow >= 0)
        m_storeCategoriesList->setCurrentRow(selectedRow);
    else
        rebuildStoreCategoryCards();
}

void MainWindow::rebuildStoreCategoryCards()
{
    auto *grid = m_storeCategoryCardsContainer
            ? qobject_cast<QGridLayout *>(m_storeCategoryCardsContainer->layout())
            : nullptr;
    if (!grid)
        return;

    while (grid->count()) {
        auto *item = grid->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QString category = m_selectedStoreCategory.trimmed();
    QVector<AppInfo> filtered;
    for (const AppInfo &app : m_storeCategoryPool) {
        bool match = false;
        for (const QString &cat : app.categories) {
            if (cat.compare(category, Qt::CaseInsensitive) == 0) {
                match = true;
                break;
            }
        }
        if (!match && !category.isEmpty()) {
            const QString haystack = (app.name + QLatin1Char(' ') + app.summary).toLower();
            match = haystack.contains(category.toLower());
        }
        if (category.isEmpty() || match)
            filtered.append(app);
    }

    const int spacing = grid->horizontalSpacing() > 0 ? grid->horizontalSpacing() : 12;
    const int availableWidth = exploreAvailableWidth(m_storeCategoryCardsContainer, grid);
    const int columns = exploreColumnCount(availableWidth, spacing);
    if (filtered.isEmpty()) {
        auto *label = new QLabel(tr("No apps found for this category."), m_storeCategoryCardsContainer);
        label->setStyleSheet(QStringLiteral("color: gray; padding: 20px;"));
        grid->addWidget(label, 0, 0, 1, columns);
        return;
    }

    for (int i = 0; i < filtered.size(); ++i) {
        const AppInfo app = filtered.at(i);
        auto *card = new AppCardWidget(m_storeCategoryCardsContainer);
        card->setApp(app);
        const int row = i / columns;
        const int col = i % columns;
        grid->addWidget(card, row, col, Qt::AlignTop | Qt::AlignHCenter);
        connect(card, &AppCardWidget::openDetailsRequested, this, [this, app]() {
            showDetailsForApp(app.id);
        });
        connect(card, &AppCardWidget::installRequested, this, [this, app]() {
            onExploreInstallRequested(app.id);
        });
    }
    for (int col = 0; col < columns; ++col)
        grid->setColumnStretch(col, 0);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_stack && m_stack->currentIndex() == 0) {
        if (m_mainContent && m_mainContent->currentIndex() == 0) {
            if (m_topSectionIndex == 0)
                rebuildExploreCards();
            else if (m_topSectionIndex == 1)
                rebuildExploreCards();
        } else if (m_mainContent && m_mainContent->currentIndex() == 1 && m_topSectionIndex == 0) {
            rebuildStoreCategoryCards();
        }
    }
}

StoreTemplate MainWindow::defaultStoreTemplate() const
{
    StoreTemplate tpl;
    tpl.id = QStringLiteral("default");
    tpl.repoId = QStringLiteral("flathub");
    tpl.displayName = tr("Flathub");
    tpl.bannerEyebrow = tr("Featured on Flathub");
    tpl.bannerEmptySummary = tr("Browse trending apps curated from Flathub.");
    tpl.featuredSummaryFallback = tr("Browse this featured app on Flathub.");
    tpl.storeUrlBase = QStringLiteral("https://flathub.org/apps/");
    tpl.feedLabels = QStringList{tr("Trending"), tr("Popular"), tr("New"), tr("Updated"), tr("Categories")};
    tpl.supportsCategories = true;
    return tpl;
}

QVector<StoreTemplate> MainWindow::allStoreTemplates() const
{
    StoreTemplate flathub = defaultStoreTemplate();

    StoreTemplate fedora;
    fedora.id = QStringLiteral("fedora-default");
    fedora.repoId = QStringLiteral("fedora");
    fedora.displayName = tr("Fedora");
    fedora.bannerEyebrow = tr("Featured on Fedora Flatpaks");
    fedora.bannerEmptySummary = tr("Browse popular apps from Fedora's Flatpak remote.");
    fedora.featuredSummaryFallback = tr("Browse this featured app from Fedora.");
    fedora.storeUrlBase = QString();
    fedora.feedLabels = QStringList{tr("Trending"), tr("Popular"), tr("New"), tr("Updated"), tr("Categories")};
    fedora.supportsCategories = true;
    fedora.hideUnlessRemotePresent = true;

    return QVector<StoreTemplate>{flathub, fedora};
}

void MainWindow::updateStoreTemplatesForRemotes(const QVector<QPair<QString, QString>> &remotes)
{
    QSet<QString> remoteNames;
    for (const auto &remote : remotes) {
        const QString name = remote.first.trimmed();
        if (!name.isEmpty())
            remoteNames.insert(name.toLower());
    }

    QVector<StoreTemplate> visible;
    visible.reserve(allStoreTemplates().size());
    for (const StoreTemplate &tpl : allStoreTemplates()) {
        if (!tpl.hideUnlessRemotePresent) {
            visible.append(tpl);
            continue;
        }
        if (remoteNames.contains(tpl.repoId.toLower()))
            visible.append(tpl);
    }
    if (visible.isEmpty())
        visible.append(defaultStoreTemplate());

    const QString previousTemplateId = m_activeStoreTemplate.id;
    m_storeTemplates = visible;
    rebuildStoreTemplateTabs();

    int nextIndex = 0;
    for (int i = 0; i < m_storeTemplates.size(); ++i) {
        if (m_storeTemplates.at(i).id == previousTemplateId) {
            nextIndex = i;
            break;
        }
    }

    if (m_storeTemplateTabs && m_storeTemplateTabs->count() > 0) {
        const bool templateChanged = m_storeTemplates.at(nextIndex).id != m_activeStoreTemplate.id;
        m_storeTemplateTabs->blockSignals(true);
        m_storeTemplateTabs->setCurrentIndex(nextIndex);
        m_storeTemplateTabs->blockSignals(false);
        if (templateChanged)
            onStoreTemplateChanged(nextIndex);
    } else if (m_storeTemplates.at(nextIndex).id != m_activeStoreTemplate.id) {
        onStoreTemplateChanged(nextIndex);
    }
}

void MainWindow::rebuildStoreTemplateTabs()
{
    if (!m_storeTemplateTabs)
        return;

    while (m_storeTemplateTabs->count() > 0)
        m_storeTemplateTabs->removeTab(0);
    for (const StoreTemplate &tpl : m_storeTemplates)
        m_storeTemplateTabs->addTab(tpl.displayName);

    const bool showSwitcher = m_storeTemplates.size() > 1;
    if (m_storeTemplateTabsHost)
        m_storeTemplateTabsHost->setVisible(showSwitcher);
}

void MainWindow::applyActiveStoreTemplateUi()
{
    if (m_storeButton)
        m_storeButton->setText(m_activeStoreTemplate.displayName);

    if (m_storeFeedTabs) {
        m_storeFeedTabs->blockSignals(true);
        const int oldCount = m_storeFeedTabs->count();
        const int currentIndex = oldCount > 0
                ? qBound(0, m_storeFeedTabs->currentIndex(), oldCount - 1)
                : 0;
        while (m_storeFeedTabs->count() > 0)
            m_storeFeedTabs->removeTab(0);
        for (const QString &label : m_activeStoreTemplate.feedLabels)
            m_storeFeedTabs->addTab(label);
        if (m_storeFeedTabs->count() > 0)
            m_storeFeedTabs->setCurrentIndex(qMin(currentIndex, m_storeFeedTabs->count() - 1));
        m_storeFeedTabs->blockSignals(false);
    }

    const QString newRepoId = m_activeStoreTemplate.repoId;
    if (!m_appliedStoreRepoId.isEmpty() && newRepoId != m_appliedStoreRepoId) {
        m_storeSuggestions.clear();
        m_storeTrending.clear();
        m_storePopular.clear();
        m_storeRecent.clear();
        m_storeUpdated.clear();
        m_storeCategoryPool.clear();
        m_selectedStoreCategory.clear();
        m_bannerApps.clear();
        m_lastExploreModelSignature.clear();
        refreshFeaturedBanner({});
    }
    m_appliedStoreRepoId = newRepoId;

    if (m_bannerTitleLabel && m_bannerSummaryLabel) {
        if (m_bannerEyebrowLabel)
            m_bannerEyebrowLabel->setText(m_activeStoreTemplate.bannerEyebrow);
        m_bannerTitleLabel->setText(tr("Discover apps"));
        m_bannerSummaryLabel->setText(m_activeStoreTemplate.bannerEmptySummary);
    }
    refreshStoreCategoriesPane();
    onStoreFeedChanged(m_storeFeedTabs ? m_storeFeedTabs->currentIndex() : 0);
}

QString MainWindow::buildStorePageUrl(const AppInfo &app) const
{
    if (!app.storePageUrl.isEmpty())
        return app.storePageUrl;
    const QString fromOrigin = m_backend->catalogPageUrlForInstalledApp(app.repoId, app.id);
    if (!fromOrigin.isEmpty())
        return fromOrigin;
    if (!m_activeStoreTemplate.storeUrlBase.isEmpty() && !app.id.isEmpty()) {
        const QUrl base(m_activeStoreTemplate.storeUrlBase);
        if (base.isValid())
            return base.toString() + app.id;
    }
    return QString();
}

void MainWindow::prepareDetailsAppInfo(AppInfo &info) const
{
    const QString catalogUrl = m_backend->catalogPageUrlForInstalledApp(info.repoId, info.id);
    if (!catalogUrl.isEmpty())
        info.storePageUrl = catalogUrl;
    else
        info.storePageUrl.clear();

    info.remoteRepoUrl = m_backend->remoteConfigUrl(info.repoId);
    m_backend->enrichAppMetadata(info);
}

QVector<AppInfo> MainWindow::installedAppsSnapshot() const
{
    QVector<AppInfo> apps;
    apps.reserve(m_installedModel->rowCount());
    for (int i = 0; i < m_installedModel->rowCount(); ++i)
        apps.append(m_installedModel->appAt(i));
    return apps;
}

void MainWindow::configureInstalledRowSource(InstalledRowWidget *row, const AppInfo &app)
{
    if (!row)
        return;
    if (TrackedBuildMatcher::isEligibleForTrackLink(app, m_trackedBuildProjects)) {
        row->setTrackGitRepoLink(true, TrackedBuildMatcher::trackLinkLabelForApp(app));
    } else {
        row->setTrackGitRepoLink(false, QString());
    }
    refreshInstalledRowTrackedUpdate(row, app);
}

void MainWindow::refreshInstalledRowTrackedUpdate(InstalledRowWidget *row, const AppInfo &app)
{
    if (!row)
        return;
    QString releaseTag;
    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.linkedAppId.trimmed() != app.id)
            continue;
        if (TrackedBuildClassifier::trackedReleaseUpdateTarget(app.version, project, &releaseTag,
                                                                 nullptr)) {
            row->setTrackedUpdateAvailable(true, releaseTag);
            return;
        }
    }
    row->setTrackedUpdateAvailable(false, QString());
}

void MainWindow::patchInstalledTrackedBuildMetadata()
{
    if (!m_installedModel || m_installedModel->rowCount() == 0)
        return;
    QVector<AppInfo> apps;
    apps.reserve(m_installedModel->rowCount());
    for (int i = 0; i < m_installedModel->rowCount(); ++i)
        apps.append(m_installedModel->appAt(i));
    m_backend->applyTrackedBuildMetadata(&apps);
    m_installedModel->setApps(apps);
    for (const AppInfo &app : apps) {
        InstalledRowWidget *row = m_installedRowsByAppId.value(app.id);
        if (!row || row->parentWidget() != m_installedContainer)
            continue;
        row->setApp(app);
        configureInstalledRowSource(row, app);
    }
}

void MainWindow::patchInstalledAppsMetadata(const QVector<AppInfo> &patches)
{
    if (patches.isEmpty())
        return;
    m_installedModel->patchApps(patches);
    for (const AppInfo &patch : patches) {
        if (InstalledRowWidget *row = m_installedRowsByAppId.value(patch.id)) {
            AppInfo merged = patch;
            for (int i = 0; i < m_installedModel->rowCount(); ++i) {
                if (m_installedModel->appAt(i).id == patch.id) {
                    merged = m_installedModel->appAt(i);
                    break;
                }
            }
            row->setApp(merged);
            configureInstalledRowSource(row, merged);
        }
    }
}

void MainWindow::openTrackedBuildDialogForApp(const AppInfo &app)
{
    const std::optional<ParsedGitRepo> parsed = TrackedBuildMatcher::parsedGitRepoFromApp(app);
    if (!parsed)
        return;

    TrackedBuildSetupHints hints;
    hints.prefilledLinkedAppId = app.id;
    hints.prefilledUrl = parsed->webUrl;

    const TrackedBuildProject *existing = TrackedBuildMatcher::findProjectByRepo(
        m_trackedBuildProjects, *parsed);
    if (existing && existing->linkedAppId.trimmed().isEmpty()) {
        TrackedBuildProject editable = *existing;
        editable.linkedAppId = app.id;
        hints.bannerText = tr("Curio believes %1 (%2) belongs to this repository, but the linked app id is blank. "
                              "Verify and click Save if correct.")
                                   .arg(app.name.isEmpty() ? app.id : app.name, app.id);
        TrackedBuildSetupDialog dialog(m_backend,
                                       TrackedBuildSetupDialog::Mode::Edit,
                                       editable,
                                       installedAppsSnapshot(),
                                       hints,
                                       this);
        connect(&dialog,
                &TrackedBuildSetupDialog::installRequested,
                this,
                &MainWindow::onTrackedBuildInstallRequested);
        if (dialog.exec() != QDialog::Accepted)
            return;
        m_backend->upsertTrackedBuildProject(dialog.resultProject());
        return;
    }

    hints.bannerText = tr("Add this repository to Tracked Builds and link %1 (%2)?")
                               .arg(app.name.isEmpty() ? app.id : app.name, app.id);
    if (!presentTrackedBuildSetupDialog(TrackedBuildSetupDialog::Mode::Add, {}, hints))
        return;
}

MainWindow::TrackedUpdateSummary MainWindow::summarizeTrackedUpdates() const
{
    TrackedUpdateSummary summary;
    if (!m_backend || !m_installedModel)
        return summary;

    const QVector<TrackedBuildProject> projects = m_backend->trackedBuildProjects();
    for (const TrackedBuildProject &project : projects) {
        if (project.linkedAppId.trimmed().isEmpty())
            continue;

        AppInfo installed;
        for (int i = 0; i < m_installedModel->rowCount(); ++i) {
            const AppInfo candidate = m_installedModel->appAt(i);
            if (candidate.id == project.linkedAppId) {
                installed = candidate;
                break;
            }
        }
        if (installed.id.isEmpty())
            continue;

        ++summary.trackedInstalledCount;
        if (!project.lastError.trimmed().isEmpty())
            summary.errors.append(QStringLiteral("%1: %2").arg(project.repoSlug, project.lastError));

        QString releaseTag;
        QString assetUrl;
        if (TrackedBuildClassifier::trackedReleaseUpdateTarget(installed.version, project,
                                                                &releaseTag, &assetUrl)) {
            ++summary.pendingUpdateCount;
        }
    }
    return summary;
}

void MainWindow::refreshTrackedUpdateBanner()
{
    if (!m_checkUpdatesStatusLabel)
        return;

    const TrackedUpdateSummary summary = summarizeTrackedUpdates();
    if (summary.pendingUpdateCount <= 0) {
        updateInstalledNavAttention();
        return;
    }

    m_checkUpdatesStatusLabel->setStyleSheet(
            QStringLiteral("color: rgba(232, 200, 120, 0.95); font-weight: 500;"));
    m_checkUpdatesStatusLabel->setText(
            summary.pendingUpdateCount == 1
                    ? tr("1 tracked update available — click Update in the list.")
                    : tr("%1 tracked updates available — click Update in the list.")
                              .arg(summary.pendingUpdateCount));
    m_checkUpdatesStatusLabel->show();
    updateInstalledNavAttention();
}

void MainWindow::updateInstalledNavAttention()
{
    if (!m_installedButton)
        return;

    const bool wantAttention = summarizeTrackedUpdates().pendingUpdateCount > 0
            && m_topSectionIndex != 2;
    m_installedButton->setProperty("updatesPending", wantAttention);

    if (wantAttention) {
        if (!m_installedNavPulseTimer) {
            m_installedNavPulseTimer = new QTimer(this);
            m_installedNavPulseTimer->setInterval(850);
            connect(m_installedNavPulseTimer, &QTimer::timeout, this, [this]() {
                if (!m_installedButton)
                    return;
                m_installedNavPulseBright = !m_installedNavPulseBright;
                m_installedButton->setProperty("pulseBright", m_installedNavPulseBright);
                m_installedButton->style()->unpolish(m_installedButton);
                m_installedButton->style()->polish(m_installedButton);
            });
        }
        if (!m_installedNavPulseTimer->isActive())
            m_installedNavPulseTimer->start();
    } else {
        if (m_installedNavPulseTimer)
            m_installedNavPulseTimer->stop();
        m_installedNavPulseBright = false;
        m_installedButton->setProperty("pulseBright", false);
        m_installedButton->style()->unpolish(m_installedButton);
        m_installedButton->style()->polish(m_installedButton);
    }
}

void MainWindow::finishCheckForUpdatesFeedback(bool userInitiated)
{
    m_checkForUpdatesPending = false;

    if (userInitiated && m_checkUpdatesButton) {
        m_checkUpdatesButton->setEnabled(true);
        m_checkUpdatesButton->setText(tr("Check for updates"));
    }
    if (!m_checkUpdatesStatusLabel || !m_backend || !m_installedModel)
        return;

    const TrackedUpdateSummary summary = summarizeTrackedUpdates();
    const QString defaultStyle = QStringLiteral("color: rgba(200,200,210,0.92);");

    if (!summary.errors.isEmpty()) {
        m_checkUpdatesStatusLabel->setStyleSheet(defaultStyle);
        m_checkUpdatesStatusLabel->setText(summary.errors.join(QStringLiteral("\n")));
        m_checkUpdatesStatusLabel->show();
        return;
    }
    if (summary.pendingUpdateCount > 0) {
        refreshTrackedUpdateBanner();
        return;
    }
    if (!userInitiated)
        return;

    m_checkUpdatesStatusLabel->setStyleSheet(defaultStyle);
    if (summary.trackedInstalledCount > 0) {
        m_checkUpdatesStatusLabel->setText(tr("All tracked apps are up to date."));
    } else {
        m_checkUpdatesStatusLabel->setText(
                tr("No installed apps are linked to tracked builds. Add repos in Settings → Tracked Builds."));
    }
    m_checkUpdatesStatusLabel->show();
}

void MainWindow::refreshTrackedBuildsList()
{
    if (!m_trackedBuildsList)
        return;
    const QString selectedId = m_trackedBuildsList->currentItem()
            ? m_trackedBuildsList->currentItem()->data(Qt::UserRole).toString()
            : QString();
    m_trackedBuildsList->clear();
    int selectedRow = -1;
    for (int i = 0; i < m_trackedBuildProjects.size(); ++i) {
        const TrackedBuildProject &project = m_trackedBuildProjects.at(i);
        QString label = QStringLiteral("%1/%2").arg(project.providerId, project.repoSlug);
        if (project.isBuiltIn())
            label = QStringLiteral("🔒 ") + label;
        if (!project.latestStableVersion.isEmpty()) {
            label += QStringLiteral("  [→ %1").arg(project.latestStableVersion);
            if (project.includePrereleases && !project.latestNightlyVersion.isEmpty())
                label += QStringLiteral(" · stable %1").arg(project.latestNightlyVersion);
            label += QLatin1Char(']');
        }
        auto *item = new QListWidgetItem(label, m_trackedBuildsList);
        item->setData(Qt::UserRole, project.id);
        if (!project.lastError.isEmpty())
            item->setToolTip(project.lastError);
        if (project.id == selectedId)
            selectedRow = i;
    }
    if (selectedRow >= 0)
        m_trackedBuildsList->setCurrentRow(selectedRow);
    else if (m_trackedBuildsList->count() > 0)
        m_trackedBuildsList->setCurrentRow(0);
    else
        onTrackedBuildSelectionChanged();
}

void MainWindow::onTrackedBuildSelectionChanged()
{
    if (!m_trackedBuildsList)
        return;
    const QListWidgetItem *item = m_trackedBuildsList->currentItem();
    if (!item) {
        if (m_trackedEditButton)
            m_trackedEditButton->setEnabled(false);
        if (m_trackedRemoveButton)
            m_trackedRemoveButton->setEnabled(false);
        return;
    }
    const QString projectId = item->data(Qt::UserRole).toString();
    const bool isBuiltin = projectId == trackedBuildBuiltinCurioId();
    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.id != projectId)
            continue;
        if (m_trackedStatusLabel) {
            m_trackedStatusLabel->setText(project.lastError.isEmpty()
                                          ? tr("Last checked: %1").arg(project.lastCheckedAtIso)
                                          : project.lastError);
        }
        if (m_trackedEditButton)
            m_trackedEditButton->setEnabled(true);
        if (m_trackedRemoveButton)
            m_trackedRemoveButton->setEnabled(!isBuiltin);
        return;
    }
    if (m_trackedEditButton)
        m_trackedEditButton->setEnabled(false);
    if (m_trackedRemoveButton)
        m_trackedRemoveButton->setEnabled(false);
}

void MainWindow::onTrackedBuildAdd()
{
    if (!presentTrackedBuildSetupDialog(TrackedBuildSetupDialog::Mode::Add))
        return;
    if (m_trackedStatusLabel)
        m_trackedStatusLabel->setText(tr("Tracked project added."));
}

void MainWindow::onTrackedBuildEdit()
{
    if (!m_trackedBuildsList || !m_trackedBuildsList->currentItem())
        return;
    const QString projectId = m_trackedBuildsList->currentItem()->data(Qt::UserRole).toString();

    TrackedBuildProject existing;
    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.id == projectId) {
            existing = project;
            break;
        }
    }
    if (existing.id.isEmpty())
        return;

    if (!presentTrackedBuildSetupDialog(TrackedBuildSetupDialog::Mode::Edit, existing))
        return;
    if (m_trackedStatusLabel)
        m_trackedStatusLabel->setText(tr("Tracked project updated."));
}

void MainWindow::onTrackedBuildRemove()
{
    if (!m_trackedBuildsList || !m_trackedBuildsList->currentItem() || !m_trackedStatusLabel)
        return;
    const QString projectId = m_trackedBuildsList->currentItem()->data(Qt::UserRole).toString();
    if (projectId.isEmpty() || projectId == trackedBuildBuiltinCurioId())
        return;
    m_backend->removeTrackedBuildProject(projectId);
    m_trackedStatusLabel->setText(tr("Tracked project removed."));
}

void MainWindow::onTrackedBuildRefresh()
{
    if (m_trackedStatusLabel)
        m_trackedStatusLabel->setText(tr("Refreshing tracked build feeds..."));
    m_backend->refreshTrackedBuilds();
}

void MainWindow::setStoreFeedLoading(bool loading)
{
    if (m_storeFeedLoading == loading)
        return;
    m_storeFeedLoading = loading;
    if (m_storeStatusLabel && m_topSectionIndex == 0) {
        if (loading) {
            const bool hasCachedFeed = !m_storeTrending.isEmpty() || !m_storePopular.isEmpty()
                    || !m_storeRecent.isEmpty() || !m_storeUpdated.isEmpty();
            if (!hasCachedFeed) {
                m_storeStatusLabel->setText(tr("Refreshing catalog…"));
                m_storeStatusLabel->setVisible(true);
            }
        } else if (m_storeStatusLabel->text() == tr("Refreshing catalog…")) {
            m_storeStatusLabel->clear();
            m_storeStatusLabel->setVisible(false);
        }
    }
    if (m_topSectionIndex == 0 && m_exploreModel->rowCount() == 0)
        scheduleExploreRebuild();
}

void MainWindow::beginStoreRefresh(bool forceRefresh)
{
    const QString repoId = m_activeStoreTemplate.repoId;
    const bool flathub = repoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0;
    if (forceRefresh || !flathub || !m_backend->flathubCollectionsFreshForToday())
        setStoreFeedLoading(true);
    m_backend->fetchStoreSuggestions(repoId, forceRefresh);
    if (flathub) {
        const int catalogDelayMs = forceRefresh ? 0 : 8000;
        QTimer::singleShot(catalogDelayMs, this, [this, repoId]() {
            m_backend->refreshCatalogIndex(repoId);
        });
    }
}

QString MainWindow::exploreModelSignature(const QVector<AppInfo> &apps)
{
    QStringList ids;
    ids.reserve(apps.size());
    for (const AppInfo &app : apps) {
        if (!app.id.isEmpty())
            ids.append(app.id);
    }
    return ids.join(QLatin1Char('|'));
}
