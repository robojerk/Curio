#include "RuntimeUpdatesRowWidget.h"

#include <QHBoxLayout>
#include <QLabel>
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
    setFixedHeight(52);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

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
    layout->addLayout(textColumn, 1);

    setRuntimeUpdateCount(0);
}

void RuntimeUpdatesRowWidget::setRuntimeUpdateCount(int count)
{
    if (count <= 0) {
        m_titleLabel->setText(tr("Runtime updates"));
        m_hintLabel->clear();
        return;
    }
    m_titleLabel->setText(
            count == 1 ? tr("1 Runtime Update") : tr("%1 Runtime Updates").arg(count));
    m_hintLabel->setText(tr("Use Update all above"));
}

void RuntimeUpdatesRowWidget::setUpdateInProgress(bool inProgress)
{
    if (inProgress) {
        m_hintLabel->setText(tr("Updating…"));
        return;
    }
    const QString title = m_titleLabel->text();
    if (title.contains(QStringLiteral("Runtime Update")))
        m_hintLabel->setText(tr("Use Update all above"));
    else
        m_hintLabel->clear();
}
