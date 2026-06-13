#pragma once

#include "models/AppInfo.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QListWidget;

/** Summary row for pending Flatpak runtime/SDK updates (Bazaar-style, expandable). */
class RuntimeUpdatesRowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RuntimeUpdatesRowWidget(QWidget *parent = nullptr);

    void setRuntimeUpdates(const QVector<AppInfo> &updates);
    void setUpdateInProgress(bool inProgress);

private:
    void updateSummaryText();
    void setExpanded(bool expanded);

    QLabel *m_titleLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QListWidget *m_detailsList = nullptr;
    QVector<AppInfo> m_updates;
    bool m_expanded = false;
    bool m_updateInProgress = false;
};
