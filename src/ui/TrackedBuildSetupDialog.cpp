#include "TrackedBuildSetupDialog.h"

#include "backend/FlatpakBackend.h"
#include "backend/GitHost.h"
#include "backend/TrackedBuildClassifier.h"
#include "backend/TrackedBuildMatcher.h"
#include "backend/TrackedBuildSource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPointer>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString previewAssetForRelease(const TrackedBuildProject &config, const TrackedBuildRelease &release)
{
    if (TrackedBuildClassifier::channelLabelForRelease(config, release) == QStringLiteral("skip"))
        return QString();
    return TrackedBuildClassifier::selectAssetUrl(config, release.assets);
}
}

TrackedBuildSetupDialog::TrackedBuildSetupDialog(FlatpakBackend *backend,
                                                 Mode mode,
                                                 const TrackedBuildProject &existing,
                                                 const QVector<AppInfo> &installedApps,
                                                 const TrackedBuildSetupHints &hints,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_backend(backend)
    , m_project(existing)
    , m_installedApps(installedApps)
{
    setWindowTitle(mode == Mode::Add ? tr("Add Git Repository") : tr("Edit Tracked Build"));
    resize(720, 580);

    m_source = new TrackedBuildSource(backend ? backend->networkAccessManager() : nullptr, this);

    auto *layout = new QVBoxLayout(this);

    m_bannerLabel = new QLabel(this);
    m_bannerLabel->setWordWrap(true);
    m_bannerLabel->setVisible(false);
    m_bannerLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: palette(alternate-base); padding: 8px; border-radius: 4px; }"));
    layout->addWidget(m_bannerLabel);

    if (mode == Mode::Add) {
        m_urlEdit = new QLineEdit(this);
        m_urlEdit->setPlaceholderText(tr("https://github.com/owner/repo"));
        if (!hints.prefilledUrl.isEmpty())
            m_urlEdit->setText(hints.prefilledUrl);
        layout->addWidget(new QLabel(tr("Repository URL"), this));
        layout->addWidget(m_urlEdit);
    } else {
        m_repoLabel = new QLineEdit(this);
        m_repoLabel->setReadOnly(true);
        layout->addWidget(new QLabel(tr("Repository"), this));
        layout->addWidget(m_repoLabel);
    }

    auto *fetchRow = new QHBoxLayout;
    m_fetchButton = new QPushButton(tr("Fetch Releases"), this);
    fetchRow->addWidget(m_fetchButton);
    fetchRow->addStretch(1);
    layout->addLayout(fetchRow);

    auto *form = new QFormLayout;
    m_linkedAppIdEdit = new QLineEdit(this);
    m_linkedAppIdEdit->setPlaceholderText(tr("e.g. io.github.example.App"));
    if (!hints.prefilledLinkedAppId.isEmpty())
        m_linkedAppIdEdit->setText(hints.prefilledLinkedAppId);
    form->addRow(tr("Linked app id"), m_linkedAppIdEdit);

    m_matchedAppLabel = new QLabel(this);
    m_matchedAppLabel->setWordWrap(true);
    m_matchedAppLabel->setVisible(false);
    m_matchedAppCombo = new QComboBox(this);
    m_matchedAppCombo->setVisible(false);
    form->addRow(m_matchedAppLabel, m_matchedAppCombo);

    m_includePrereleasesCheck = new QCheckBox(tr("Include pre-releases"), this);
    m_includePrereleasesCheck->setChecked(false);
    m_includePrereleasesCheck->setToolTip(
            tr("When off, skips GitHub pre-releases and uses the newest stable release with a "
               "matching asset. When on, uses whichever was published most recently."));
    form->addRow(m_includePrereleasesCheck);

    m_assetFilterRegexEdit = new QLineEdit(this);
    m_assetFilterRegexEdit->setPlaceholderText(tr("e.g. .*x86_64.*\\.flatpak$"));
    form->addRow(tr("Asset filter regex"), m_assetFilterRegexEdit);

    m_includeZipCheck = new QCheckBox(tr("Include zip release assets"), this);
    m_zippedFlatpakRegexEdit = new QLineEdit(this);
    m_zippedFlatpakRegexEdit->setPlaceholderText(tr("Inner .flatpak filename filter"));
    m_autoArchCheck = new QCheckBox(tr("Prefer assets matching host architecture"), this);
    m_autoArchCheck->setChecked(true);
    form->addRow(m_includeZipCheck);
    form->addRow(tr("Zip inner filter"), m_zippedFlatpakRegexEdit);
    form->addRow(m_autoArchCheck);
    layout->addLayout(form);

    m_advancedGroup = new QGroupBox(tr("Advanced filters"), this);
    m_advancedGroup->setCheckable(true);
    m_advancedGroup->setChecked(false);
    auto *advancedForm = new QFormLayout(m_advancedGroup);
    m_releaseTitleFilterRegexEdit = new QLineEdit(m_advancedGroup);
    m_releaseTitleFilterRegexEdit->setPlaceholderText(
        tr("Optional: release tag or title must match this pattern"));
    m_excludeRegexEdit = new QLineEdit(m_advancedGroup);
    advancedForm->addRow(tr("Release title filter regex"), m_releaseTitleFilterRegexEdit);
    advancedForm->addRow(tr("Exclude regex"), m_excludeRegexEdit);
    layout->addWidget(m_advancedGroup);

    if (existing.providerId == QStringLiteral("gitlab")) {
        auto *gitlabNote = new QLabel(
            tr("GitLab does not expose a pre-release flag like GitHub. Releases are treated as "
               "normal unless you use the title filter above."),
            this);
        gitlabNote->setWordWrap(true);
        gitlabNote->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
        layout->addWidget(gitlabNote);
    }

    m_previewTable = new QTableWidget(0, 4, this);
    m_previewTable->setHorizontalHeaderLabels({tr("Tag"), tr("Title"), tr("Channel"), tr("Asset")});
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(new QLabel(tr("Release preview"), this));
    layout->addWidget(m_previewTable, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_statusLabel);

    auto *buttons = new QHBoxLayout;
    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    m_installLatestButton = new QPushButton(tr("Install Latest"), this);
    m_installLatestButton->setEnabled(false);
    m_saveButton = new QPushButton(tr("Save"), this);
    buttons->addStretch(1);
    buttons->addWidget(cancelButton);
    buttons->addWidget(m_installLatestButton);
    buttons->addWidget(m_saveButton);
    layout->addLayout(buttons);

    connect(m_fetchButton, &QPushButton::clicked, this, &TrackedBuildSetupDialog::onFetchClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &TrackedBuildSetupDialog::onSaveClicked);
    connect(m_installLatestButton, &QPushButton::clicked, this, &TrackedBuildSetupDialog::onInstallLatestClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_matchedAppCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0)
            return;
        const QString appId = m_matchedAppCombo->currentData().toString();
        if (!appId.isEmpty())
            m_linkedAppIdEdit->setText(appId);
    });

    const auto hookPreview = [this]() { updatePreview(); };
    for (QLineEdit *edit : {m_linkedAppIdEdit, m_assetFilterRegexEdit, m_releaseTitleFilterRegexEdit,
                            m_excludeRegexEdit, m_zippedFlatpakRegexEdit}) {
        connect(edit, &QLineEdit::textChanged, this, hookPreview);
    }
    for (QCheckBox *box : {m_includePrereleasesCheck, m_includeZipCheck, m_autoArchCheck}) {
        connect(box, &QCheckBox::toggled, this, hookPreview);
    }
    connect(m_advancedGroup, &QGroupBox::toggled, this, hookPreview);

    if (!hints.bannerText.isEmpty()) {
        m_bannerLabel->setText(hints.bannerText);
        m_bannerLabel->setVisible(true);
    }

    if (mode == Mode::Edit) {
        populateFromProject(existing);
        if (existing.isBuiltIn())
            applyBuiltInEditRestrictions();
        onFetchClicked();
    } else if (!hints.prefilledUrl.isEmpty()) {
        onFetchClicked();
    }
}

std::optional<ParsedGitRepo> TrackedBuildSetupDialog::currentParsedRepo() const
{
    if (m_mode == Mode::Add && m_urlEdit)
        return GitHost::parseUrl(m_urlEdit->text().trimmed());
    if (!m_project.repoSlug.isEmpty()) {
        ParsedGitRepo repo;
        repo.repoSlug = m_project.repoSlug;
        repo.host = GitHost::hostForKind(m_project.providerId == QStringLiteral("gitlab")
                                                 ? GitHostKind::GitLab
                                                 : m_project.providerId == QStringLiteral("codeberg")
                                                 ? GitHostKind::Codeberg
                                                 : GitHostKind::GitHub);
        repo.webUrl = GitHost::canonicalWebUrl(repo);
        return repo;
    }
    return std::nullopt;
}

void TrackedBuildSetupDialog::applyInstalledAppMatches(const ParsedGitRepo &repo)
{
    const QVector<AppInfo> matches = TrackedBuildMatcher::findInstalledAppsForRepo(m_installedApps, repo);
    m_matchedAppCombo->clear();
    m_matchedAppLabel->setVisible(false);
    m_matchedAppCombo->setVisible(false);

    if (matches.isEmpty())
        return;

    for (const AppInfo &app : matches) {
        const QString label = app.name.isEmpty() ? app.id : QStringLiteral("%1 (%2)").arg(app.name, app.id);
        m_matchedAppCombo->addItem(label, app.id);
    }

    if (m_linkedAppIdEdit->text().trimmed().isEmpty() && !matches.first().id.isEmpty())
        m_linkedAppIdEdit->setText(matches.first().id);

    if (matches.size() == 1) {
        m_matchedAppLabel->setText(tr("Matched installed app:"));
        m_matchedAppLabel->setVisible(true);
        m_matchedAppCombo->setVisible(true);
    } else {
        m_matchedAppLabel->setText(tr("Multiple installed apps match this repository:"));
        m_matchedAppLabel->setVisible(true);
        m_matchedAppCombo->setVisible(true);
    }
}

void TrackedBuildSetupDialog::applyBuiltInEditRestrictions()
{
    if (!m_project.isBuiltIn())
        return;

    const auto lockLine = [](QLineEdit *edit) {
        if (!edit)
            return;
        edit->setReadOnly(true);
        edit->setEnabled(false);
    };
    const auto lockCheck = [](QCheckBox *box) {
        if (!box)
            return;
        box->setEnabled(false);
    };

    lockLine(m_linkedAppIdEdit);
    lockLine(m_assetFilterRegexEdit);
    lockLine(m_zippedFlatpakRegexEdit);
    lockCheck(m_includeZipCheck);
    lockCheck(m_autoArchCheck);
    if (m_advancedGroup)
        m_advancedGroup->setEnabled(false);

    if (m_includePrereleasesCheck)
        m_includePrereleasesCheck->setEnabled(true);

    if (m_bannerLabel) {
        m_bannerLabel->setText(
                tr("Built-in entry for this app. Only “Include pre-releases” can be changed; other "
                   "fields are fixed."));
        m_bannerLabel->setVisible(true);
    }
}

void TrackedBuildSetupDialog::populateFromProject(const TrackedBuildProject &project)
{
    m_project = project;
    if (m_repoLabel)
        m_repoLabel->setText(QStringLiteral("%1 (%2)").arg(project.repoSlug, project.providerId));
    m_linkedAppIdEdit->setText(project.linkedAppId);
    m_includePrereleasesCheck->setChecked(project.includePrereleases);
    m_assetFilterRegexEdit->setText(trackedBuildEffectiveAssetFilterRegex(project));
    const QString titleFilter = trackedBuildEffectiveReleaseTitleFilterRegex(project);
    if (!titleFilter.isEmpty()) {
        m_advancedGroup->setChecked(true);
        m_releaseTitleFilterRegexEdit->setText(titleFilter);
    }
    m_excludeRegexEdit->setText(project.excludeRegex);
    if (!project.excludeRegex.isEmpty())
        m_advancedGroup->setChecked(true);
    m_includeZipCheck->setChecked(project.includeZipAssets);
    m_zippedFlatpakRegexEdit->setText(project.zippedFlatpakFilterRegex);
    m_autoArchCheck->setChecked(project.autoFlatpakFilterByArch);
}

void TrackedBuildSetupDialog::setBusy(bool busy)
{
    m_fetchButton->setEnabled(!busy);
    m_saveButton->setEnabled(!busy);
    if (!busy)
        updateInstallLatestButton();
    else if (m_installLatestButton)
        m_installLatestButton->setEnabled(false);
    if (m_urlEdit)
        m_urlEdit->setEnabled(!busy);
}

void TrackedBuildSetupDialog::applySuggestions(const TrackedBuildSuggestions &suggestions)
{
    if (m_linkedAppIdEdit->text().trimmed().isEmpty() && !suggestions.linkedAppId.isEmpty())
        m_linkedAppIdEdit->setText(suggestions.linkedAppId);
    if (m_assetFilterRegexEdit->text().trimmed().isEmpty())
        m_assetFilterRegexEdit->setText(suggestions.assetFilterRegex);
    if (m_zippedFlatpakRegexEdit->text().trimmed().isEmpty())
        m_zippedFlatpakRegexEdit->setText(suggestions.zippedFlatpakFilterRegex);
    if (suggestions.includeZipAssets)
        m_includeZipCheck->setChecked(true);
}

void TrackedBuildSetupDialog::onFetchClicked()
{
    TrackedBuildProject probe = buildProjectFromFields();
    std::optional<ParsedGitRepo> parsed = currentParsedRepo();
    if (m_mode == Mode::Add) {
        if (!parsed) {
            m_statusLabel->setText(tr("Enter a valid GitHub, GitLab, or Codeberg repository URL."));
            return;
        }
        probe.providerId = parsed->providerId();
        probe.repoSlug = parsed->repoSlug;
        probe.id = GitHost::projectId(*parsed);
        m_project.providerId = probe.providerId;
        m_project.repoSlug = probe.repoSlug;
        m_project.id = probe.id;
    }

    if (probe.repoSlug.isEmpty()) {
        m_statusLabel->setText(tr("Repository slug is required."));
        return;
    }

    setBusy(true);
    m_statusLabel->setText(tr("Fetching releases…"));
    const QPointer<TrackedBuildSetupDialog> self(this);
    m_source->fetchReleases(probe, [self, probe, parsed](const QVector<TrackedBuildRelease> &releases,
                                                         const QString &error) {
        if (!self)
            return;
        self->setBusy(false);
        if (!error.isEmpty()) {
            self->m_statusLabel->setText(error);
            return;
        }
        self->m_releases = releases;
        self->m_statusLabel->setText(self->tr("Loaded %1 releases.").arg(releases.size()));
        if (parsed)
            self->applyInstalledAppMatches(*parsed);
        self->applySuggestions(TrackedBuildClassifier::suggestFromReleases(releases));
        self->updatePreview();
    });
}

TrackedBuildProject TrackedBuildSetupDialog::buildProjectFromFields() const
{
    TrackedBuildProject project = m_project;
    project.linkedAppId = m_linkedAppIdEdit->text().trimmed();
    project.includePrereleases = m_includePrereleasesCheck->isChecked();
    project.trackStable = true;
    project.trackNightly = project.includePrereleases;
    project.assetFilterRegex = m_assetFilterRegexEdit->text().trimmed();
    project.releaseTitleFilterRegex = m_advancedGroup->isChecked()
            ? m_releaseTitleFilterRegexEdit->text().trimmed()
            : QString();
    project.excludeRegex = m_advancedGroup->isChecked() ? m_excludeRegexEdit->text().trimmed() : QString();
    project.includeZipAssets = m_includeZipCheck->isChecked();
    project.zippedFlatpakFilterRegex = m_zippedFlatpakRegexEdit->text().trimmed();
    project.autoFlatpakFilterByArch = m_autoArchCheck->isChecked();
    project.stableTagRegex.clear();
    project.nightlyTagRegex.clear();
    project.stableAssetRegex.clear();
    project.nightlyAssetRegex.clear();
    return project;
}

bool TrackedBuildSetupDialog::validateFields(QString *errorMessage) const
{
    const auto checkRegex = [&](const QString &pattern, const QString &label) -> bool {
        if (pattern.isEmpty())
            return true;
        QRegularExpression rx(pattern);
        if (!rx.isValid()) {
            if (errorMessage)
                *errorMessage = tr("Invalid %1: %2").arg(label, rx.errorString());
            return false;
        }
        return true;
    };

    if (!checkRegex(m_assetFilterRegexEdit->text().trimmed(), tr("asset filter regex")))
        return false;
    if (m_advancedGroup->isChecked()) {
        if (!checkRegex(m_releaseTitleFilterRegexEdit->text().trimmed(), tr("release title filter regex"))
                || !checkRegex(m_excludeRegexEdit->text().trimmed(), tr("exclude regex"))) {
            return false;
        }
    }
    if (!checkRegex(m_zippedFlatpakRegexEdit->text().trimmed(), tr("zip inner filter")))
        return false;
    return true;
}

void TrackedBuildSetupDialog::updatePreview()
{
    if (!m_previewTable)
        return;
    const TrackedBuildProject config = buildProjectFromFields();
    QString installTargetTag;
    TrackedBuildClassifier::pickLatestInstallTarget(config, m_releases, &installTargetTag, nullptr);
    QVector<TrackedBuildRelease> sorted = m_releases;
    std::sort(sorted.begin(), sorted.end(), [](const TrackedBuildRelease &a, const TrackedBuildRelease &b) {
        return a.publishedAtIso > b.publishedAtIso;
    });
    const int previewCount = qMin(5, sorted.size());
    m_previewTable->setRowCount(previewCount);
    for (int i = 0; i < previewCount; ++i) {
        const TrackedBuildRelease &release = sorted.at(i);
        auto *tagItem = new QTableWidgetItem(release.tagName);
        const QString prereleaseHint = release.isPrerelease ? tr("API: pre-release") : tr("API: release");
        tagItem->setToolTip(prereleaseHint);
        m_previewTable->setItem(i, 0, tagItem);
        m_previewTable->setItem(i, 1, new QTableWidgetItem(release.title));
        const QString channel = TrackedBuildClassifier::channelLabelForRelease(config, release);
        const QString tag = release.tagName.trimmed();
        const bool isTarget = !installTargetTag.isEmpty()
                && QString::compare(tag, installTargetTag, Qt::CaseInsensitive) == 0;
        m_previewTable->setItem(i, 2, new QTableWidgetItem(isTarget ? tr("→ install") : channel));
        const QString asset = previewAssetForRelease(config, release);
        auto *assetItem = new QTableWidgetItem(asset);
        assetItem->setToolTip(asset);
        m_previewTable->setItem(i, 3, assetItem);
    }
    updateInstallLatestButton();
}

void TrackedBuildSetupDialog::updateInstallLatestButton()
{
    if (!m_installLatestButton)
        return;

    if (!m_backend || m_releases.isEmpty()) {
        m_installLatestButton->setEnabled(false);
        m_installLatestButton->setText(tr("Install Latest"));
        m_installLatestButton->setToolTip(tr("Fetch releases first."));
        return;
    }

    const TrackedBuildProject config = buildProjectFromFields();
    QString releaseTag;
    QString assetUrl;
    if (!TrackedBuildClassifier::latestTrackedInstallTarget(config, m_releases, &releaseTag, &assetUrl)
            || assetUrl.isEmpty()) {
        m_installLatestButton->setEnabled(false);
        m_installLatestButton->setText(tr("Install Latest"));
        m_installLatestButton->setToolTip(
                tr("No installable release matches the current filters. Adjust tracking options or fetch again."));
        return;
    }

    m_installLatestButton->setEnabled(true);
    m_installLatestButton->setText(tr("Install %1").arg(releaseTag));
    m_installLatestButton->setToolTip(assetUrl);
}

bool TrackedBuildSetupDialog::finalizeProjectFromFields(TrackedBuildProject *project, QString *errorMessage) const
{
    if (!project) {
        if (errorMessage)
            *errorMessage = tr("Internal error.");
        return false;
    }

    if (!validateFields(errorMessage))
        return false;

    if (m_project.isBuiltIn()) {
        *project = m_project;
        project->includePrereleases = m_includePrereleasesCheck->isChecked();
        project->trackStable = true;
        project->trackNightly = project->includePrereleases;
        if (!m_releases.isEmpty()) {
            TrackedBuildClassifier::classifyReleases(*project, m_releases, project);
            project->lastCheckedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            project->lastError.clear();
        }
        return true;
    }

    *project = buildProjectFromFields();
    if (m_mode == Mode::Add) {
        const std::optional<ParsedGitRepo> parsed = GitHost::parseUrl(m_urlEdit->text().trimmed());
        if (!parsed) {
            if (errorMessage)
                *errorMessage = tr("Enter a valid repository URL.");
            return false;
        }
        project->providerId = parsed->providerId();
        project->repoSlug = parsed->repoSlug;
        project->id = GitHost::projectId(*parsed);
    } else {
        project->id = m_project.id;
        project->providerId = m_project.providerId;
        project->repoSlug = m_project.repoSlug;
    }

    if (project->repoSlug.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Repository slug is required.");
        return false;
    }

    if (!m_releases.isEmpty()) {
        TrackedBuildClassifier::classifyReleases(*project, m_releases, project);
        project->lastCheckedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        project->lastError.clear();
    }

    return true;
}

void TrackedBuildSetupDialog::onInstallLatestClicked()
{
    if (!m_backend) {
        QMessageBox::warning(this, tr("Install unavailable"), tr("Flatpak backend is unavailable."));
        return;
    }

    TrackedBuildProject project;
    QString errorMessage;
    if (!finalizeProjectFromFields(&project, &errorMessage)) {
        QMessageBox::warning(this, tr("Invalid configuration"), errorMessage);
        return;
    }

    QString releaseTag;
    QString assetUrl;
    if (!TrackedBuildClassifier::latestTrackedInstallTarget(project, m_releases, &releaseTag, &assetUrl)
            || assetUrl.isEmpty()) {
        QMessageBox::information(
                this,
                tr("No matching release"),
                tr("No installable release matches the current filters. Try adjusting tracking options or fetch releases again."));
        return;
    }

    const QString linkedAppId = project.linkedAppId.trimmed();
    if (linkedAppId.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Linked app required"),
                             tr("Set a linked Flatpak app id before installing."));
        return;
    }

    m_project = project;
    emit installRequested(project, assetUrl, linkedAppId, releaseTag);
    accept();
}

void TrackedBuildSetupDialog::onSaveClicked()
{
    TrackedBuildProject project;
    QString errorMessage;
    if (!finalizeProjectFromFields(&project, &errorMessage)) {
        QMessageBox::warning(this, tr("Invalid configuration"), errorMessage);
        return;
    }

    m_project = project;
    accept();
}
