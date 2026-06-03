#pragma once

#include <QString>
#include <QStringList>

struct StoreTemplate
{
    QString id;
    QString repoId;
    QString displayName;
    QString bannerEyebrow;
    QString bannerEmptySummary;
    QString featuredSummaryFallback;
    QString storeUrlBase;
    QStringList feedLabels;
    bool supportsCategories = true;
    /** When true, hide this store unless a matching flatpak remote is configured. */
    bool hideUnlessRemotePresent = false;
};

