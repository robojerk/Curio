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
#include <QDesktopServices>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QResizeEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfoList>
#include <QFile>
#include <QVBoxLayout>
#include <QWidget>
#include <QStringList>
#include <QIcon>
#include <QColor>
#include <QSet>

#include "backend/FlatpakBackend.h"
#include "models/AppListModel.h"
#include "models/OperationModel.h"
#include "ui/AppCardWidget.h"
#include "ui/AppDetailsWidget.h"
#include "ui/InstalledRowWidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_backend = new FlatpakBackend(this);
    m_exploreModel = new AppListModel(this);
    m_installedModel = new AppListModel(this);
    m_operationModel = new OperationModel(this);

    setupUi();
    connectBackend();

    refreshAllData();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 8, 10, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(central);

    m_bannerWidget = new QFrame(this);
    m_bannerWidget->setObjectName(QStringLiteral("featuredBanner"));
    m_bannerWidget->setFixedHeight(160);
    m_bannerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bannerWidget->setStyleSheet(QStringLiteral(
        "#featuredBanner {"
        "  background-color: #2ea56b;"
        "  border-radius: 12px;"
        "}"
    ));
    auto *bannerLayout = new QHBoxLayout(m_bannerWidget);
    bannerLayout->setContentsMargins(20, 16, 20, 16);
    bannerLayout->setSpacing(16);

    auto *bannerTextLayout = new QVBoxLayout;
    auto *bannerEyebrow = new QLabel(tr("Featured on Flathub"), m_bannerWidget);
    bannerEyebrow->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-weight: 600;"));
    m_bannerTitleLabel = new QLabel(tr("Discover apps"), m_bannerWidget);
    m_bannerTitleLabel->setStyleSheet(QStringLiteral("color: white; font-size: 24px; font-weight: 700;"));
    m_bannerSummaryLabel = new QLabel(tr("Browse trending apps curated from Flathub."), m_bannerWidget);
    m_bannerSummaryLabel->setWordWrap(true);
    m_bannerSummaryLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.92);"));
    bannerTextLayout->addWidget(bannerEyebrow);
    bannerTextLayout->addWidget(m_bannerTitleLabel);
    bannerTextLayout->addWidget(m_bannerSummaryLabel);
    bannerTextLayout->addStretch();

    m_bannerIconLabel = new QLabel(m_bannerWidget);
    m_bannerIconLabel->setFixedSize(128, 128);
    m_bannerIconLabel->setAlignment(Qt::AlignCenter);
    m_bannerIconLabel->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.08); border-radius: 12px;"));
    const QIcon defaultBannerIcon = QIcon::fromTheme(QStringLiteral("applications-other"));
    m_bannerIconLabel->setPixmap(defaultBannerIcon.pixmap(96, 96));

    bannerLayout->addLayout(bannerTextLayout, 1);
    bannerLayout->addWidget(m_bannerIconLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_topNav = new QWidget(this);
    auto *topNavLayout = new QHBoxLayout(m_topNav);
    topNavLayout->setContentsMargins(0, 0, 0, 0);
    topNavLayout->setSpacing(8);
    topNavLayout->addStretch();
    m_flathubButton = new QPushButton(tr("Flathub"), m_topNav);
    m_searchButton = new QPushButton(tr("Search"), m_topNav);
    m_installedButton = new QPushButton(tr("Installed Apps"), m_topNav);
    m_settingsButton = new QPushButton(tr("Settings"), m_topNav);
    for (QPushButton *btn : {m_flathubButton, m_searchButton, m_installedButton, m_settingsButton}) {
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
    }
    m_flathubButton->setChecked(true);
    topNavLayout->addWidget(m_flathubButton);
    topNavLayout->addWidget(m_searchButton);
    topNavLayout->addWidget(m_installedButton);
    topNavLayout->addWidget(m_settingsButton);
    topNavLayout->addStretch();
    rootLayout->addWidget(m_topNav);
    rootLayout->addWidget(m_bannerWidget);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search apps…"));
    rootLayout->addWidget(m_searchEdit);

    m_flathubFeedTabs = new QTabBar(this);
    m_flathubFeedTabs->addTab(tr("Trending"));
    m_flathubFeedTabs->addTab(tr("Popular"));
    m_flathubFeedTabs->addTab(tr("New"));
    m_flathubFeedTabs->addTab(tr("Updated"));
    m_flathubFeedTabs->addTab(tr("Categories"));
    m_flathubFeedTabs->setExpanding(false);
    rootLayout->addWidget(m_flathubFeedTabs);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // Page 0: main content stack (Flathub explore, Installed, Operations)
    m_mainContent = new QStackedWidget(this);

    // Explore tab: scroll area with grid of cards
    auto *exploreScroll = new QScrollArea(this);
    exploreScroll->setWidgetResizable(true);
    exploreScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_exploreContainer = new QWidget(this);
    auto *exploreGrid = new QGridLayout(m_exploreContainer);
    exploreGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    exploreGrid->setHorizontalSpacing(12);
    exploreGrid->setVerticalSpacing(12);
    exploreGrid->setContentsMargins(8, 8, 8, 8);
    m_exploreContainer->setLayout(exploreGrid);
    exploreScroll->setWidget(m_exploreContainer);
    m_mainContent->addWidget(exploreScroll);

    // Flathub categories tab content
    auto *categoriesScroll = new QScrollArea(this);
    categoriesScroll->setWidgetResizable(true);
    categoriesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_flathubCategoriesContainer = new QWidget(this);
    auto *categoriesLayout = new QVBoxLayout(m_flathubCategoriesContainer);
    categoriesLayout->setContentsMargins(8, 8, 8, 8);
    categoriesLayout->setSpacing(10);
    auto *feedCategoriesPanes = new QHBoxLayout;
    feedCategoriesPanes->setContentsMargins(0, 0, 0, 0);
    feedCategoriesPanes->setSpacing(10);

    auto *leftPane = new QFrame(m_flathubCategoriesContainer);
    leftPane->setFrameShape(QFrame::StyledPanel);
    leftPane->setStyleSheet(QStringLiteral("QFrame { border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; }"));
    auto *leftPaneLayout = new QVBoxLayout(leftPane);
    leftPaneLayout->setContentsMargins(10, 10, 10, 10);
    leftPaneLayout->setSpacing(10);
    auto *leftTitle = new QLabel(tr("Categories"), leftPane);
    leftTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
    leftPaneLayout->addWidget(leftTitle);
    m_flathubCategoriesList = new QListWidget(leftPane);
    m_flathubCategoriesList->setMinimumWidth(220);
    leftPaneLayout->addWidget(m_flathubCategoriesList, 1);

    auto *rightPane = new QFrame(m_flathubCategoriesContainer);
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
    m_flathubCategoryCardsContainer = new QWidget(categoryAppsScroll);
    auto *categoryCardsGrid = new QGridLayout(m_flathubCategoryCardsContainer);
    categoryCardsGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    categoryCardsGrid->setHorizontalSpacing(12);
    categoryCardsGrid->setVerticalSpacing(12);
    categoryCardsGrid->setContentsMargins(0, 0, 0, 0);
    categoryAppsScroll->setWidget(m_flathubCategoryCardsContainer);
    rightPaneLayout->addWidget(categoryAppsScroll, 1);

    connect(m_flathubCategoriesList, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        m_selectedFlathubCategory = text.trimmed();
        rebuildFlathubCategoryCards();
    });

    feedCategoriesPanes->addWidget(leftPane, 1);
    feedCategoriesPanes->addWidget(rightPane, 3);
    categoriesLayout->addLayout(feedCategoriesPanes, 1);
    categoriesScroll->setWidget(m_flathubCategoriesContainer);
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
        tr("Categories"),
        tr("Login with Flathub"),
        tr("Remotes"),
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

    auto *categoriesPage = new QWidget(m_settingsDetailStack);
    auto *categoriesPageLayout = new QVBoxLayout(categoriesPage);
    auto *categoriesTitle = new QLabel(tr("Categories"), categoriesPage);
    categoriesTitle->setFont(sectionTitleFont);
    auto *categoriesHelp = new QLabel(tr("Browse app categories."), categoriesPage);
    categoriesHelp->setWordWrap(true);
    categoriesHelp->setStyleSheet(QStringLiteral("color: gray;"));
    categoriesPageLayout->addWidget(categoriesTitle);
    categoriesPageLayout->addWidget(categoriesHelp);

    auto *categoriesGridHost = new QWidget(categoriesPage);
    auto *categoriesGrid = new QGridLayout(categoriesGridHost);
    categoriesGrid->setContentsMargins(0, 0, 0, 0);
    categoriesGrid->setHorizontalSpacing(10);
    categoriesGrid->setVerticalSpacing(10);
    struct CategoryPill {
        const char *label;
        const char *color;
    };
    const QVector<CategoryPill> categoryPills = {
        {"Development", "#4C8DFF"},
        {"Games", "#FF7A59"},
        {"Graphics", "#A167FF"},
        {"Audio & Video", "#12B886"},
        {"Office", "#F59F00"},
        {"Education", "#2F9E44"},
        {"Science", "#0CA678"},
        {"Utilities", "#5C7CFA"},
        {"Internet", "#15AABF"},
        {"System", "#868E96"}
    };
    for (int i = 0; i < categoryPills.size(); ++i) {
        const CategoryPill &pill = categoryPills.at(i);
        auto *btn = new QPushButton(tr(pill.label), categoriesGridHost);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "background-color: %1;"
            "color: white;"
            "font-weight: 600;"
            "border: none;"
            "border-radius: 10px;"
            "padding: 10px 14px;"
            "text-align: left;"
            "}"
            "QPushButton:hover {"
            "background-color: %2;"
            "}"
        ).arg(QString::fromUtf8(pill.color),
              QStringLiteral("%1").arg(QString::fromUtf8(pill.color))));
        const int row = i / 2;
        const int col = i % 2;
        categoriesGrid->addWidget(btn, row, col);
    }
    categoriesPageLayout->addWidget(categoriesGridHost);
    categoriesPageLayout->addStretch(1);
    m_settingsDetailStack->addWidget(categoriesPage);

    auto *loginPage = new QWidget(m_settingsDetailStack);
    auto *loginPageLayout = new QVBoxLayout(loginPage);
    auto *loginTitle = new QLabel(tr("Login with Flathub"), loginPage);
    loginTitle->setFont(sectionTitleFont);
    auto *loginHelp = new QLabel(
        tr("Curio loads provider options from Flathub and supports OAuth providers like GitHub, GitLab, GNOME GitLab, and KDE GitLab."),
        loginPage);
    loginHelp->setWordWrap(true);
    loginHelp->setStyleSheet(QStringLiteral("color: gray;"));
    loginPageLayout->addWidget(loginTitle);
    loginPageLayout->addWidget(loginHelp);
    const QStringList providerLabels{tr("GitHub"), tr("GitLab"), tr("GNOME GitLab"), tr("KDE GitLab")};
    for (const QString &providerLabel : providerLabels) {
        auto *providerButton = new QPushButton(providerLabel, loginPage);
        loginPageLayout->addWidget(providerButton);
        connect(providerButton, &QPushButton::clicked, this, &MainWindow::onLoginWithFlathubTriggered);
    }
    auto *openLoginButton = new QPushButton(tr("Open Flathub Login in Browser"), loginPage);
    loginPageLayout->addWidget(openLoginButton);
    connect(openLoginButton, &QPushButton::clicked, this, &MainWindow::onLoginWithFlathubTriggered);
    loginPageLayout->addStretch(1);
    m_settingsDetailStack->addWidget(loginPage);

    auto *remotesPage = new QWidget(m_settingsDetailStack);
    auto *remotesPageLayout = new QVBoxLayout(remotesPage);
    auto *remotesTitle = new QLabel(tr("Remotes"), remotesPage);
    remotesTitle->setFont(sectionTitleFont);
    auto *remotesHelp = new QLabel(tr("Add custom Flatpak remotes and view currently configured remotes."), remotesPage);
    remotesHelp->setWordWrap(true);
    remotesHelp->setStyleSheet(QStringLiteral("color: gray;"));
    m_remotesList = new QListWidget(remotesPage);
    m_remotesList->setMinimumHeight(180);
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
    m_leftoverDataList->setMinimumHeight(180);
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
    settingsLayout->addLayout(settingsPanesLayout);
    settingsLayout->addStretch(1);
    m_stack->addWidget(m_settingsPage);

    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &MainWindow::onSearchDebounceFired);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_flathubButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(0); });
    connect(m_searchButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(1); });
    connect(m_installedButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(2); });
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() { onTopTabChanged(3); });
    connect(m_flathubFeedTabs, &QTabBar::currentChanged, this, &MainWindow::onFlathubFeedChanged);
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
        openSettingsSection(5);
    });
    auto *showShortcutsAltShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+/")), this);
    showShortcutsAltShortcut->setContext(Qt::ApplicationShortcut);
    connect(showShortcutsAltShortcut, &QShortcut::activated, this, [openSettingsSection]() {
        openSettingsSection(5);
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
    refreshLeftoverDataPane();
    onRefreshRemotesTriggered();
    onTopTabChanged(0);
    m_flathubFeedTabs->setCurrentIndex(0);

    setWindowTitle(tr("Curio"));
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

    connect(m_backend, &FlatpakBackend::searchResultsUpdated,
            this, [this](const QVector<AppInfo> &apps) {
                m_exploreModel->setApps(apps);
                rebuildExploreCards();
            });

    connect(m_backend, &FlatpakBackend::flathubSuggestionsUpdated,
            this, [this](const QVector<AppInfo> &apps) {
                m_flathubSuggestions = apps;
                refreshFeaturedBanner(apps);
                refreshFlathubCategoriesPane();
                if (m_topSectionIndex == 0) {
                    onFlathubFeedChanged(m_flathubFeedTabs->currentIndex());
                }
            });
    connect(m_backend, &FlatpakBackend::flathubCollectionsUpdated,
            this, [this](const QVector<AppInfo> &trending,
                         const QVector<AppInfo> &popular,
                         const QVector<AppInfo> &recent,
                         const QVector<AppInfo> &updated) {
                m_flathubTrending = trending;
                m_flathubPopular = popular;
                m_flathubRecent = recent;
                m_flathubUpdated = updated;
                refreshFeaturedBanner(m_flathubTrending.isEmpty() ? m_flathubSuggestions : m_flathubTrending);
                refreshFlathubCategoriesPane();
                if (m_topSectionIndex == 0)
                    onFlathubFeedChanged(m_flathubFeedTabs->currentIndex());
            });

    connect(m_backend, &FlatpakBackend::operationStarted,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
            });
    connect(m_backend, &FlatpakBackend::operationProgress,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
            });
    connect(m_backend, &FlatpakBackend::operationFinished,
            this, [this](const Operation &op) {
                m_operationModel->addOrUpdate(op);
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
}

void MainWindow::rebuildExploreCards()
{
    auto *grid = qobject_cast<QGridLayout *>(m_exploreContainer->layout());
    if (!grid)
        return;

    m_exploreContainer->setUpdatesEnabled(false);

    // Clear existing cards
    while (grid->count()) {
        auto *item = grid->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const int minCardWidth = 210;
    const int spacing = grid->horizontalSpacing() > 0 ? grid->horizontalSpacing() : 12;
    const int availableWidth = qMax(0, m_exploreContainer->width() - grid->contentsMargins().left() - grid->contentsMargins().right());
    const int columns = qMax(1, (availableWidth + spacing) / (minCardWidth + spacing));
    const int count = m_exploreModel->rowCount();
    if (count == 0) {
        auto *emptyLabel = new QLabel(tr("No results found."), m_exploreContainer);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: gray; padding: 24px;"));
        grid->addWidget(emptyLabel, 0, 0, 1, columns);
        m_exploreContainer->setUpdatesEnabled(true);
        return;
    }
    for (int i = 0; i < count; ++i) {
        AppInfo app = m_exploreModel->appAt(i);
        auto *card = new AppCardWidget(m_exploreContainer);
        card->setApp(app);
        const int row = i / columns;
        const int col = i % columns;
        grid->addWidget(card, row, col);

        connect(card, &AppCardWidget::openDetailsRequested, this, [this, app]() {
            showDetailsForApp(app.id);
        });
        connect(card, &AppCardWidget::installRequested, this, [this, app]() {
            onExploreInstallRequested(app.id);
        });
    }

    m_exploreContainer->setUpdatesEnabled(true);
}

void MainWindow::rebuildInstalledRows()
{
    auto *vbox = qobject_cast<QVBoxLayout *>(m_installedContainer->layout());
    if (!vbox)
        return;

    m_installedContainer->setUpdatesEnabled(false);

    // Keep the first row ("Check for updates" button) pinned.
    while (vbox->count() > 1) {
        auto *item = vbox->takeAt(1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const int count = m_installedModel->rowCount();
    for (int i = 0; i < count; ++i) {
        AppInfo app = m_installedModel->appAt(i);
        auto *row = new InstalledRowWidget(m_installedContainer);
        row->setApp(app);
        vbox->addWidget(row);

        connect(row, &InstalledRowWidget::openDetailsRequested, this, [this, app]() {
            showDetailsForApp(app.id);
        });
        connect(row, &InstalledRowWidget::uninstallRequested, this, [this, app]() {
            onInstalledUninstallRequested(app.id);
        });
    }

    m_installedContainer->setUpdatesEnabled(true);
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
        rebuildExploreCards();
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
    if (m_flathubButton && m_searchButton && m_installedButton && m_settingsButton) {
        if (index == 0)
            m_flathubButton->setChecked(true);
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
        m_flathubFeedTabs->setVisible(false);
        m_stack->setCurrentIndex(2);
        return;
    }

    if (m_bannerWidget)
        m_bannerWidget->setVisible(index == 0);
    m_searchEdit->setVisible(index == 1);
    m_flathubFeedTabs->setVisible(index == 0);
    m_stack->setCurrentIndex(0);

    if (index == 2) {
        m_mainContent->setCurrentIndex(2); // Installed
        return;
    }

    if (index == 1) {
        m_mainContent->setCurrentIndex(0); // Explore for search results
        if (!m_searchEdit->text().trimmed().isEmpty())
            onSearchDebounceFired();
        else {
            m_exploreModel->setApps({});
            rebuildExploreCards();
        }
        return;
    }

    // Flathub tab
    onFlathubFeedChanged(m_flathubFeedTabs->currentIndex());
}

void MainWindow::onFlathubFeedChanged(int index)
{
    if (m_topSectionIndex != 0)
        return;

    if (index == 4) {
        m_mainContent->setCurrentIndex(1); // Flathub categories
        refreshFlathubCategoriesPane();
        return;
    }

    m_mainContent->setCurrentIndex(0); // Explore feed cards
    switch (index) {
    case 1:
        m_exploreModel->setApps(m_flathubPopular);
        break;
    case 2:
        m_exploreModel->setApps(m_flathubRecent);
        break;
    case 3:
        m_exploreModel->setApps(m_flathubUpdated);
        break;
    case 0:
    default:
        m_exploreModel->setApps(m_flathubTrending.isEmpty() ? m_flathubSuggestions : m_flathubTrending);
        break;
    }
    rebuildExploreCards();
}

void MainWindow::onSettingsSectionChanged(int row)
{
    if (!m_settingsDetailStack)
        return;
    if (row < 0 || row >= m_settingsDetailStack->count())
        return;
    m_settingsDetailStack->setCurrentIndex(row);
    if (row == 3)
        onRefreshRemotesTriggered();
    if (row == 4)
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
        m_searchEdit->setVisible(false);
        m_flathubFeedTabs->setVisible(false);
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
        m_searchEdit->setVisible(false);
        m_flathubFeedTabs->setVisible(false);
        m_detailsWidget->setApp(info);
        m_stack->setCurrentIndex(1);
    }
}

void MainWindow::onExploreInstallRequested(const QString &appId)
{
    m_backend->install(appId);
    showDetailsForApp(appId);
}

void MainWindow::onInstalledUninstallRequested(const QString &appId)
{
    m_backend->uninstall(appId);
}

void MainWindow::onCheckForUpdatesTriggered()
{
    refreshAllData();
}

void MainWindow::onLoginWithFlathubTriggered()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://flathub.org/settings")));
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
    m_backend->fetchFlathubSuggestions();
    const QString query = m_searchEdit->text().trimmed();
    if (m_topSectionIndex == 1 && !query.isEmpty())
        m_backend->search(query);
}

void MainWindow::refreshFeaturedBanner(const QVector<AppInfo> &apps)
{
    if (!m_bannerTitleLabel || !m_bannerSummaryLabel || !m_bannerIconLabel)
        return;

    if (apps.isEmpty()) {
        m_bannerTitleLabel->setText(tr("Discover apps"));
        m_bannerSummaryLabel->setText(tr("Browse trending apps curated from Flathub."));
        const QIcon icon = QIcon::fromTheme(QStringLiteral("applications-other"));
        m_bannerIconLabel->setPixmap(icon.pixmap(96, 96));
        return;
    }

    const AppInfo app = apps.first();
    m_bannerTitleLabel->setText(app.name.isEmpty() ? app.id : app.name);
    m_bannerSummaryLabel->setText(app.summary.isEmpty()
                                  ? tr("Browse this featured app on Flathub.")
                                  : app.summary);

    QIcon icon;
    if (!app.iconName.isEmpty())
        icon = QIcon::fromTheme(app.iconName);
    if (icon.isNull() && !app.id.isEmpty())
        icon = QIcon::fromTheme(app.id);
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral("applications-other"));
    m_bannerIconLabel->setPixmap(icon.pixmap(96, 96));
}

void MainWindow::refreshFlathubCategoriesPane()
{
    if (!m_flathubCategoriesList)
        return;

    QVector<AppInfo> pool;
    pool.reserve(m_flathubTrending.size() + m_flathubPopular.size() + m_flathubRecent.size() + m_flathubUpdated.size());
    QSet<QString> seenIds;
    auto appendUnique = [&pool, &seenIds](const QVector<AppInfo> &src) {
        for (const AppInfo &app : src) {
            if (app.id.isEmpty() || seenIds.contains(app.id))
                continue;
            seenIds.insert(app.id);
            pool.append(app);
        }
    };
    appendUnique(m_flathubTrending);
    appendUnique(m_flathubPopular);
    appendUnique(m_flathubRecent);
    appendUnique(m_flathubUpdated);
    if (pool.isEmpty())
        appendUnique(m_flathubSuggestions);

    m_flathubCategoryPool = pool;

    QSet<QString> categoriesSet;
    for (const AppInfo &app : m_flathubCategoryPool) {
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

    const QString prevSelection = m_selectedFlathubCategory;
    m_flathubCategoriesList->clear();
    const QStringList colors = {
        QStringLiteral("#4C8DFF"),
        QStringLiteral("#FF7A59"),
        QStringLiteral("#A167FF"),
        QStringLiteral("#12B886"),
        QStringLiteral("#F59F00"),
        QStringLiteral("#2F9E44"),
        QStringLiteral("#0CA678"),
        QStringLiteral("#5C7CFA"),
        QStringLiteral("#15AABF"),
        QStringLiteral("#868E96")
    };
    for (int i = 0; i < categories.size(); ++i) {
        auto *item = new QListWidgetItem(categories.at(i), m_flathubCategoriesList);
        item->setForeground(Qt::white);
        item->setBackground(QColor(colors.at(i % colors.size())));
    }

    int selectedRow = -1;
    if (!prevSelection.isEmpty()) {
        for (int i = 0; i < m_flathubCategoriesList->count(); ++i) {
            if (m_flathubCategoriesList->item(i)->text().compare(prevSelection, Qt::CaseInsensitive) == 0) {
                selectedRow = i;
                break;
            }
        }
    }
    if (selectedRow < 0 && m_flathubCategoriesList->count() > 0)
        selectedRow = 0;
    if (selectedRow >= 0)
        m_flathubCategoriesList->setCurrentRow(selectedRow);
    else
        rebuildFlathubCategoryCards();
}

void MainWindow::rebuildFlathubCategoryCards()
{
    auto *grid = m_flathubCategoryCardsContainer
            ? qobject_cast<QGridLayout *>(m_flathubCategoryCardsContainer->layout())
            : nullptr;
    if (!grid)
        return;

    while (grid->count()) {
        auto *item = grid->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QString category = m_selectedFlathubCategory.trimmed();
    QVector<AppInfo> filtered;
    for (const AppInfo &app : m_flathubCategoryPool) {
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

    const int minCardWidth = 210;
    const int spacing = grid->horizontalSpacing() > 0 ? grid->horizontalSpacing() : 12;
    const int availableWidth = qMax(0, m_flathubCategoryCardsContainer->width() - grid->contentsMargins().left() - grid->contentsMargins().right());
    const int columns = qMax(1, (availableWidth + spacing) / (minCardWidth + spacing));
    if (filtered.isEmpty()) {
        auto *label = new QLabel(tr("No apps found for this category."), m_flathubCategoryCardsContainer);
        label->setStyleSheet(QStringLiteral("color: gray; padding: 20px;"));
        grid->addWidget(label, 0, 0, 1, columns);
        return;
    }

    for (int i = 0; i < filtered.size(); ++i) {
        const AppInfo app = filtered.at(i);
        auto *card = new AppCardWidget(m_flathubCategoryCardsContainer);
        card->setApp(app);
        const int row = i / columns;
        const int col = i % columns;
        grid->addWidget(card, row, col);
        connect(card, &AppCardWidget::openDetailsRequested, this, [this, app]() {
            showDetailsForApp(app.id);
        });
        connect(card, &AppCardWidget::installRequested, this, [this, app]() {
            onExploreInstallRequested(app.id);
        });
    }
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
            rebuildFlathubCategoryCards();
        }
    }
}
