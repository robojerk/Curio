#pragma once

#include "FlatpakInstallationService.h"
#include "FlatpakScope.h"
#include "models/Operation.h"

#include <QObject>
#include <functional>

struct _FlatpakTransaction;
typedef struct _FlatpakTransaction FlatpakTransaction;
struct _GError;
typedef struct _GError GError;

class FlatpakTransactionRunner : public QObject
{
    Q_OBJECT
public:
    explicit FlatpakTransactionRunner(FlatpakInstallationService *installations,
                                        QObject *parent = nullptr);

    void installFromRemote(const Operation &op,
                           const QString &remoteName,
                           FlatpakScope scope = FlatpakScope::User);
    void uninstallRef(const Operation &op, FlatpakScope scope);
    void updateRef(const Operation &op, FlatpakScope scope);
    void updateFlatpakRef(const Operation &op, FlatpakScope scope, const QString &flatpakRef);
    void installBundle(const Operation &op,
                       const QString &bundlePath,
                       FlatpakScope scope,
                       bool replaceExisting,
                       std::function<void()> onFinishedCleanup = {});

signals:
    void operationProgress(const Operation &op);
    void operationFinished(const Operation &op);

private:
    struct TransactionContext {
        Operation op;
        std::function<void()> cleanup;
    };

    void runTransaction(FlatpakScope scope,
                        const std::function<bool(FlatpakTransaction *, GError **)> &buildTransaction,
                        TransactionContext context);

    static QString friendlyError(GError *error);
    static void connectProgressHandlers(FlatpakTransaction *transaction,
                                        FlatpakTransactionRunner *runner,
                                        Operation op,
                                        void **bridgeOut);

    FlatpakInstallationService *m_installations = nullptr;
    QAtomicInt m_activeTransactions{0};
public:
    /** Block until active transactions reach zero or timeout (ms). */
    void waitForIdle(int timeoutMs = 5000);
};
