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
    static constexpr int PreferredWidth = 320;
    static constexpr int PreferredHeight = 100;

    explicit AppCardWidget(QWidget *parent = nullptr);

    void setApp(const AppInfo &info);
    void patchIcon(const AppInfo &info);
    void refreshIcon();

    QString appId() const { return m_info.id; }

signals:
    void openDetailsRequested();
    void installRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPushButton *m_installButton = nullptr;
    AppInfo m_info;
};
