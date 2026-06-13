#include "RuntimeUpdatesRowWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

RuntimeUpdatesRowWidget::RuntimeUpdatesRowWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("runtimeUpdatesRow"));
    setStyleSheet(QStringLiteral(
            "#runtimeUpdatesRow {"
            "  background: rgba(255,255,255,0.04);"
            "  border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 10px;"
            "}"));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(12, 8, 12, 8);
    outerLayout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(8);

    m_toggleButton = new QPushButton(QStringLiteral("▸"), this);
    m_toggleButton->setFixedSize(28, 28);
    m_toggleButton->setFlat(true);
    m_toggleButton->setEnabled(false);
    m_toggleButton->setToolTip(tr("Show runtimes with available updates"));
    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        setExpanded(!m_expanded);
    });

    auto *textColumn = new QVBoxLayout;
    textColumn->setSpacing(0);
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: rgba(200,200,210,0.75);"));
    textColumn->addWidget(m_titleLabel);
    textColumn->addWidget(m_hintLabel);

    headerRow->addWidget(m_toggleButton);
    headerRow->addLayout(textColumn, 1);
    outerLayout->addLayout(headerRow);

    m_detailsList = new QListWidget(this);
    m_detailsList->setFrameShape(QFrame::NoFrame);
    m_detailsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_detailsList->setVisible(false);
    outerLayout->addWidget(m_detailsList);

    setRuntimeUpdates({});
}

void RuntimeUpdatesRowWidget::setRuntimeUpdates(const QVector<AppInfo> &updates)
{
    m_updates = updates;
    m_detailsList->clear();

    for (const AppInfo &entry : m_updates) {
        const QString name = entry.name.isEmpty() ? entry.id : entry.name;
        const QString version = entry.version.trimmed();
        const QString line = version.isEmpty()
                ? name
                : QStringLiteral("%1 (%2)").arg(name).arg(version);
        m_detailsList->addItem(line);
    }

    const bool hasUpdates = !m_updates.isEmpty();
    m_toggleButton->setEnabled(hasUpdates);
    if (!hasUpdates)
        setExpanded(false);

    updateSummaryText();
}

void RuntimeUpdatesRowWidget::setUpdateInProgress(bool inProgress)
{
    m_updateInProgress = inProgress;
    updateSummaryText();
}

void RuntimeUpdatesRowWidget::setExpanded(bool expanded)
{
    m_expanded = expanded && !m_updates.isEmpty();
    m_detailsList->setVisible(m_expanded);
    m_toggleButton->setText(m_expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
}

void RuntimeUpdatesRowWidget::updateSummaryText()
{
    const int count = m_updates.size();
    if (count <= 0) {
        m_titleLabel->setText(tr("Runtime updates"));
        m_hintLabel->clear();
        return;
    }

    m_titleLabel->setText(
            count == 1 ? tr("1 Runtime Update") : tr("%1 Runtime Updates").arg(count));
    if (m_updateInProgress)
        m_hintLabel->setText(tr("Updating…"));
    else
        m_hintLabel->setText(tr("Use Update all above"));
}
