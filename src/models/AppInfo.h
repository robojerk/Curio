#pragma once

#include <QString>
#include <QStringList>

struct AppInfo
{
    QString id;
    QString name;
    QString summary;
    QString version;
    QString iconName;
    bool installed = false;

    // Optional; filled from AppStream when available
    QString longDescription;
    QStringList screenshotUrls;
    QString iconUrl;
    QString developerName;
    QString projectLicense;
    QString homepageUrl;
    QString bugtrackerUrl;
    QString donateUrl;
    QString helpUrl;
    QString translateUrl;
    QStringList categories;
};

