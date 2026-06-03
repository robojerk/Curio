#pragma once

#include "models/AppInfo.h"

#include <QObject>
#include <QSet>

class AppStreamProvider : public QObject
{
    Q_OBJECT
public:
    explicit AppStreamProvider(QObject *parent = nullptr);

    /** Returns true if AppStream Qt is available and the pool loaded. */
    bool isAvailable() const;

    /**
     * Enrich \a info with long description and screenshot URLs from AppStream when
     * a component for info.id is found. Idempotent; only fills empty fields.
     */
    void enrich(AppInfo &info) const;

    ~AppStreamProvider() override;

private:
    void enrichFromAppStreamPool(AppInfo &info) const;
    void enrichFromInstalledMetainfo(AppInfo &info) const;
    void ensurePoolLoaded() const;
    bool debugEnabled() const;

    mutable bool m_available = false;
    mutable bool m_poolTried = false;
    mutable QSet<QString> m_loggedMisses;
    struct PoolHolder;
    mutable std::unique_ptr<PoolHolder> m_pool;
};
