#include "FlatpakTransactionRunner.h"
#include "FlatpakGlibInclude.h"

#include <QMetaObject>
#include <QtConcurrent/QtConcurrent>
#include <QAtomicInteger>
#include <QElapsedTimer>
#include <QThread>

namespace {
struct ProgressBridge {
    FlatpakTransactionRunner *runner = nullptr;
    Operation op;
};

void onProgressChanged(FlatpakTransactionProgress *progress, ProgressBridge *bridge)
{
    if (!bridge || !bridge->runner)
        return;

    Operation progressOp = bridge->op;
    progressOp.progress = flatpak_transaction_progress_get_progress(progress);
    const char *status = flatpak_transaction_progress_get_status(progress);
    if (status && status[0] != '\0')
        progressOp.message = QString::fromUtf8(status);

    FlatpakTransactionRunner *runner = bridge->runner;
    QMetaObject::invokeMethod(
            bridge->runner,
            [runner, progressOp]() { emit runner->operationProgress(progressOp); },
            Qt::QueuedConnection);
}

void onNewOperation(FlatpakTransaction *transaction,
                    FlatpakTransactionOperation *operation,
                    FlatpakTransactionProgress *progress,
                    ProgressBridge *bridge)
{
    Q_UNUSED(transaction);
    Q_UNUSED(operation);
    if (!bridge || !progress)
        return;

    flatpak_transaction_progress_set_update_frequency(progress, 100);
    g_signal_connect(progress, "changed", G_CALLBACK(onProgressChanged), bridge);
}

QString buildInstallRef(FlatpakInstallation *installation, const QByteArray &appId)
{
    Q_UNUSED(installation);
    const char *arch = flatpak_get_default_arch();
    if (!arch)
        arch = "x86_64";
    return QStringLiteral("app/%1/%2/stable").arg(QString::fromUtf8(appId.constData()), QString::fromUtf8(arch));
}
}

FlatpakTransactionRunner::FlatpakTransactionRunner(FlatpakInstallationService *installations,
                                                   QObject *parent)
    : QObject(parent)
    , m_installations(installations)
{
}

QString FlatpakTransactionRunner::friendlyError(GError *error)
{
    if (!error || !error->message)
        return FlatpakTransactionRunner::tr("Operation failed");
    const QString message = QString::fromUtf8(error->message);
    if (message.contains(QStringLiteral("not allowed for user"), Qt::CaseInsensitive)
            || message.contains(QStringLiteral("InstallBundle not allowed"), Qt::CaseInsensitive)) {
        return FlatpakTransactionRunner::tr(
                "Administrator authorization is required for system-wide Flatpak changes.");
    }
    if (message.contains(QStringLiteral("already installed"), Qt::CaseInsensitive)) {
        return FlatpakTransactionRunner::tr(
                "This version is already installed. Try uninstalling first or choose a different release.");
    }
    if (message.contains(QStringLiteral("Runtime not found"), Qt::CaseInsensitive)
            || (message.contains(QStringLiteral("runtime"), Qt::CaseInsensitive)
                && message.contains(QStringLiteral("not found"), Qt::CaseInsensitive))) {
        return FlatpakTransactionRunner::tr(
                "Bundle install failed because a required Flatpak runtime is missing. "
                "Try installing the app’s runtime remote first, then retry.");
    }
    if (message.contains(QStringLiteral("Request dismissed"), Qt::CaseInsensitive)
            || message.contains(QStringLiteral("Not authorized"), Qt::CaseInsensitive)) {
        return FlatpakTransactionRunner::tr("Administrator authorization was cancelled or denied.");
    }
    return message;
}

void FlatpakTransactionRunner::connectProgressHandlers(FlatpakTransaction *transaction,
                                                       FlatpakTransactionRunner *runner,
                                                       Operation op,
                                                       void **bridgeOut)
{
    auto *bridge = new ProgressBridge{runner, op};
    g_signal_connect(transaction, "new-operation", G_CALLBACK(onNewOperation), bridge);
    if (bridgeOut)
        *bridgeOut = bridge;
}

void FlatpakTransactionRunner::runTransaction(
        FlatpakScope scope,
        const std::function<bool(FlatpakTransaction *, GError **)> &buildTransaction,
        TransactionContext context)
{
    FlatpakInstallation *installation = m_installations
            ? m_installations->interactiveInstallation(scope)
            : nullptr;
    if (!installation) {
        Operation failed = context.op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Flatpak installation (%1) is unavailable")
                                 .arg(flatpakScopeToString(scope));
        emit operationFinished(failed);
        if (context.cleanup)
            context.cleanup();
        return;
    }

    FlatpakInstallation *installationRef = installation;
    // Track active transaction so shutdown can wait for libflatpak work to finish.
    m_activeTransactions.fetchAndAddRelaxed(1);
    (void) QtConcurrent::run([this, installationRef, buildTransaction, context]() {
        struct Defer { std::function<void()> f; ~Defer(){ if (f) f(); } } defer{[this](){ m_activeTransactions.fetchAndAddRelaxed(-1); }};
        g_autoptr(GError) error = nullptr;
        g_autoptr(FlatpakTransaction) transaction =
                flatpak_transaction_new_for_installation(installationRef, nullptr, &error);
        if (!transaction) {
            Operation failed = context.op;
            failed.status = OperationStatus::Failed;
            failed.message = friendlyError(error);
            QMetaObject::invokeMethod(this, [this, failed, cleanup = context.cleanup]() {
                emit operationFinished(failed);
                if (cleanup)
                    cleanup();
            }, Qt::QueuedConnection);
            return;
        }

        ProgressBridge *progressBridge = nullptr;
        connectProgressHandlers(transaction, this, context.op, reinterpret_cast<void **>(&progressBridge));

        if (!buildTransaction(transaction, &error)) {
            delete progressBridge;
            Operation failed = context.op;
            failed.status = OperationStatus::Failed;
            failed.message = friendlyError(error);
            QMetaObject::invokeMethod(this, [this, failed, cleanup = context.cleanup]() {
                emit operationFinished(failed);
                if (cleanup)
                    cleanup();
            }, Qt::QueuedConnection);
            return;
        }

        const gboolean ok = flatpak_transaction_run(transaction, nullptr, &error);
        delete progressBridge;

        Operation done = context.op;
        done.status = ok ? OperationStatus::Succeeded : OperationStatus::Failed;
        if (ok) {
            switch (done.type) {
            case OperationType::Install:
                done.message = tr("Installed successfully");
                break;
            case OperationType::Update:
                done.message = tr("Updated successfully");
                break;
            case OperationType::Uninstall:
                done.message = tr("Uninstalled successfully");
                break;
            }
        } else {
            done.message = friendlyError(error);
        }

        QMetaObject::invokeMethod(this, [this, done, cleanup = context.cleanup]() {
            emit operationFinished(done);
            if (cleanup)
                cleanup();
        }, Qt::QueuedConnection);
    });
}

void FlatpakTransactionRunner::waitForIdle(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (m_activeTransactions.loadRelaxed() > 0) {
        if (timeoutMs >= 0 && t.elapsed() > timeoutMs)
            break;
        QThread::msleep(20);
    }
}

void FlatpakTransactionRunner::installFromRemote(const Operation &op,
                                                 const QString &remoteName,
                                                 FlatpakScope scope)
{
    const QString remote = remoteName.trimmed().isEmpty()
            ? QStringLiteral("flathub")
            : remoteName.trimmed();
    const QByteArray appId = op.appId.toUtf8();
    const QByteArray remoteBytes = remote.toUtf8();

    FlatpakInstallation *installation = m_installations
            ? m_installations->interactiveInstallation(scope)
            : nullptr;
    const QString ref = buildInstallRef(installation, appId);
    const QByteArray refBytes = ref.toUtf8();

    runTransaction(scope,
                   [remoteBytes, refBytes](FlatpakTransaction *transaction, GError **error) {
                       return flatpak_transaction_add_install(
                               transaction,
                               remoteBytes.constData(),
                               refBytes.constData(),
                               nullptr,
                               error);
                   },
                   TransactionContext{op, {}});
}

void FlatpakTransactionRunner::uninstallRef(const Operation &op, FlatpakScope scope)
{
    const QString ref = m_installations->refForAppId(op.appId, scope);
    if (ref.isEmpty()) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Could not resolve installed ref for %1").arg(op.appId);
        emit operationFinished(failed);
        return;
    }

    const QByteArray refBytes = ref.toUtf8();
    runTransaction(scope,
                   [refBytes](FlatpakTransaction *transaction, GError **error) {
                       return flatpak_transaction_add_uninstall(transaction, refBytes.constData(), error);
                   },
                   TransactionContext{op, {}});
}

void FlatpakTransactionRunner::updateRef(const Operation &op, FlatpakScope scope)
{
    const QString ref = m_installations->refForAppId(op.appId, scope);
    updateFlatpakRef(op, scope, ref);
}

void FlatpakTransactionRunner::updateFlatpakRef(const Operation &op,
                                                FlatpakScope scope,
                                                const QString &flatpakRef)
{
    const QString ref = flatpakRef.trimmed();
    if (ref.isEmpty()) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Could not resolve installed ref for %1").arg(op.appId);
        emit operationFinished(failed);
        return;
    }

    const QByteArray refBytes = ref.toUtf8();
    runTransaction(scope,
                   [refBytes](FlatpakTransaction *transaction, GError **error) {
                       return flatpak_transaction_add_update(
                               transaction, refBytes.constData(), nullptr, nullptr, error);
                   },
                   TransactionContext{op, {}});
}

void FlatpakTransactionRunner::installBundle(const Operation &op,
                                             const QString &bundlePath,
                                             FlatpakScope scope,
                                             bool replaceExisting,
                                             std::function<void()> onFinishedCleanup)
{
    const QString path = bundlePath.trimmed();
    if (path.isEmpty()) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Bundle path is empty");
        emit operationFinished(failed);
        if (onFinishedCleanup)
            onFinishedCleanup();
        return;
    }

    const QString bundleRef = m_installations ? m_installations->bundleFormattedRef(path) : QString();
    const QStringList installedRefs = m_installations
            ? m_installations->installedRefStringsForAppId(op.appId, scope)
            : QStringList();

    QStringList refsToUninstall;
    bool sameRefInstalled = false;
    for (const QString &installedRef : installedRefs) {
        if (!bundleRef.isEmpty() && installedRef == bundleRef) {
            sameRefInstalled = true;
            continue;
        }
        refsToUninstall.append(installedRef);
    }

    const bool useReinstall = replaceExisting && refsToUninstall.isEmpty() && sameRefInstalled;

    runTransaction(scope,
                   [path, refsToUninstall, useReinstall](FlatpakTransaction *transaction, GError **error) {
                       for (const QString &ref : refsToUninstall) {
                           const QByteArray refBytes = ref.toUtf8();
                           if (!flatpak_transaction_add_uninstall(
                                       transaction, refBytes.constData(), error)) {
                               return FALSE;
                           }
                       }

                       if (useReinstall)
                           flatpak_transaction_set_reinstall(transaction, TRUE);

                       g_autoptr(GFile) file = g_file_new_for_path(path.toUtf8().constData());
                       return flatpak_transaction_add_install_bundle(transaction, file, nullptr, error);
                   },
                   TransactionContext{op, std::move(onFinishedCleanup)});
}
