#pragma once

#include <QFrame>

#include "models/AppInfo.h"

class QLabel;
class QMouseEvent;
class QNetworkAccessManager;
class QPushButton;

class AppCardWidget : public QFrame
{
    Q_OBJECT
public:
    explicit AppCardWidget(QWidget *parent = nullptr);

    void setApp(const AppInfo &info);

signals:
    void openDetailsRequested();
    void installRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPushButton *m_installButton = nullptr;
    AppInfo m_info;
};
