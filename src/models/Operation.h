#pragma once

#include <QString>

enum class OperationType {
    Install,
    Uninstall,
    Update
};

enum class OperationStatus {
    Pending,
    Running,
    Succeeded,
    Failed
};

struct Operation
{
    QString appId;
    QString appName;
    OperationType type = OperationType::Install;
    OperationStatus status = OperationStatus::Pending;
    int progress = 0; // 0-100
    QString message;
};

