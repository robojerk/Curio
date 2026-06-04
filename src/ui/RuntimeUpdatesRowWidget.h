#pragma once

#include <QWidget>

class QLabel;

/** Summary row for pending Flatpak runtime/SDK updates (Bazaar-style, informational). */
class RuntimeUpdatesRowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RuntimeUpdatesRowWidget(QWidget *parent = nullptr);

    void setRuntimeUpdateCount(int count);
    void setUpdateInProgress(bool inProgress);

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
};
