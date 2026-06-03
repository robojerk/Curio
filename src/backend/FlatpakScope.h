#pragma once

#include <QString>

enum class FlatpakScope {
    User,
    System,
};

inline QString flatpakScopeToString(FlatpakScope scope)
{
    return scope == FlatpakScope::System ? QStringLiteral("system") : QStringLiteral("user");
}

inline FlatpakScope flatpakScopeFromString(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0
            ? FlatpakScope::System
            : FlatpakScope::User;
}
