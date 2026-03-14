#pragma once

#include <QWidget>

#include "models/AppInfo.h"

class QLabel;
class QMouseEvent;
class QPushButton;

class InstalledRowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InstalledRowWidget(QWidget *parent = nullptr);

    void setApp(const AppInfo &info);

signals:
    void openDetailsRequested();
    void uninstallRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_uninstallButton = nullptr;
    AppInfo m_info;
};
