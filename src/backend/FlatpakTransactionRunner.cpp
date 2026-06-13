#include "FlatpakTransactionRunner.h"
#include "FlatpakGlibInclude.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QDeadlineTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

#ifdef CURIO_SANDBOXED_LIBFLATPAK
#include <QRegularExpression>
#endif

#ifdef CURIO_SANDBOXED_LIBFLATPAK
#include "SandboxedFlatpakEnv.h"
#include <QProcess>
#endif

#include <algorithm>
#include <atomic>

namespace {

bool flatpakDebugEnabled()
{
    return qEnvironmentVariableIsSet("CURIO_FLATPAK_DEBUG");
}

void logFlatpakDebug(const char *message, const Operation &op, const QString &detail = {})
{
    if (!flatpakDebugEnabled())
        return;
    QString line = QStringLiteral("[FlatpakTransaction] %1 appId=%2 type=%3")
                           .arg(QString::fromUtf8(message), op.appId)
                           .arg(static_cast<int>(op.type));
    if (!detail.isEmpty())
        line += QStringLiteral(" detail=") + detail;
    qInfo().noquote() << line;
}

void logFlatpakFailure(const Operation &op, const QString &detail, const QString &outputTail = {})
{
    qWarning().noquote() << "[FlatpakTransaction] failed appId=" << op.appId
                         << "type=" << static_cast<int>(op.type)
                         << "message=" << (detail.isEmpty() ? op.message : detail);
    if (!outputTail.isEmpty()) {
        qWarning().noquote() << "[FlatpakTransaction] recent output:\n"
                             << outputTail;
    }
}

QString tailOutputLines(const QString &text, int maxLines = 20)
{
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() <= maxLines)
        return lines.join(QLatin1Char('\n'));
    return lines.mid(lines.size() - maxLines).join(QLatin1Char('\n'));
}

bool isInterestingHostFlatpakLine(const QString &line)
{
    if (line.isEmpty())
        return false;
    return line.contains(QStringLiteral("Installing"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("Downloading"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("Receiving"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("Updating"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("apply_extra"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("error"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("flatpak-"), Qt::CaseInsensitive)
            || line.contains(QLatin1Char('%'));
}

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

    if (flatpakDebugEnabled() && status && status[0] != '\0') {
        logFlatpakDebug("progress",
                        bridge->op,
                        QStringLiteral("%1% %2").arg(progressOp.progress).arg(progressOp.message));
    }

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
    if (bridge && operation && flatpakDebugEnabled()) {
        const char *ref = flatpak_transaction_operation_get_ref(operation);
        logFlatpakDebug("new-operation", bridge->op, ref ? QString::fromUtf8(ref) : QString());
    }
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

#ifdef CURIO_SANDBOXED_LIBFLATPAK
QString summarizeHostFlatpakOutput(const QByteArray &output)
{
    const QString text = QString::fromUtf8(output);
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (trimmed.contains(QStringLiteral("error"), Qt::CaseInsensitive)
                && !trimmed.contains(QStringLiteral("optional summary"), Qt::CaseInsensitive)
                && !trimmed.contains(QStringLiteral("non-fatal"), Qt::CaseInsensitive)) {
            return trimmed;
        }
    }
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

void emitHostInstallProgress(FlatpakTransactionRunner *runner, Operation op, const QString &statusLine)
{
    if (statusLine.isEmpty())
        return;
    if (flatpakDebugEnabled() && isInterestingHostFlatpakLine(statusLine))
        logFlatpakDebug("host-output", op, statusLine);

    Operation progressOp = op;
    progressOp.message = statusLine;
    static const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));
    const QRegularExpressionMatch match = percentPattern.match(statusLine);
    if (match.hasMatch())
        progressOp.progress = std::clamp(match.captured(1).toInt(), 0, 100);
    QMetaObject::invokeMethod(
            runner,
            [runner, progressOp]() { emit runner->operationProgress(progressOp); },
            Qt::QueuedConnection);
}

void processHostFlatpakLine(FlatpakTransactionRunner *runner,
                            const Operation &op,
                            const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!isInterestingHostFlatpakLine(trimmed))
        return;
    emitHostInstallProgress(runner, op, trimmed);
}

void runHostFlatpakInstall(FlatpakTransactionRunner *runner,
                           const Operation &op,
                           const QString &remote,
                           FlatpakScope scope)
{
    QElapsedTimer timer;
    timer.start();

    QStringList args = {QStringLiteral("install"), QStringLiteral("-y"), QStringLiteral("--noninteractive")};
    if (flatpakDebugEnabled())
        args << QStringLiteral("--verbose");
    if (scope == FlatpakScope::User)
        args << QStringLiteral("--user");
    else
        args << QStringLiteral("--system");
    args << remote << op.appId;

    logFlatpakDebug("host-install", op, args.join(QLatin1Char(' ')));

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(SandboxedFlatpakEnv::flatpakExecutable(), args);
    if (!proc.waitForStarted(30'000)) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = FlatpakTransactionRunner::tr("Could not start host flatpak install.");
        logFlatpakFailure(failed, failed.message);
        QMetaObject::invokeMethod(
                runner,
                [runner, failed]() { emit runner->operationFinished(failed); },
                Qt::QueuedConnection);
        return;
    }

    QByteArray collected;
    qsizetype processed = 0;
    auto drainLines = [&]() {
        while (processed < collected.size()) {
            const qsizetype newline = collected.indexOf('\n', processed);
            if (newline < 0)
                break;
            const QByteArray lineBytes = collected.mid(processed, newline - processed);
            processed = newline + 1;
            processHostFlatpakLine(runner, op, QString::fromUtf8(lineBytes));
        }
    };

    while (proc.state() != QProcess::NotRunning) {
        if (proc.waitForReadyRead(500))
            collected += proc.readAll();
        drainLines();
    }
    collected += proc.readAll();
    proc.waitForFinished();
    drainLines();
    if (processed < collected.size())
        processHostFlatpakLine(runner, op, QString::fromUtf8(collected.mid(processed)));

    const QString fullOutput = QString::fromUtf8(collected);
    const QString outputTail = tailOutputLines(fullOutput);
    const qint64 elapsedMs = timer.elapsed();

    Operation done = op;
    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        done.status = OperationStatus::Succeeded;
        done.progress = 100;
        done.message = FlatpakTransactionRunner::tr("Installed successfully");
        logFlatpakDebug("host-install-finished",
                        done,
                        QStringLiteral("ok elapsedMs=%1 exitCode=0").arg(elapsedMs));
        if (flatpakDebugEnabled() && !outputTail.isEmpty()) {
            qInfo().noquote() << "[FlatpakTransaction] host output tail appId=" << done.appId
                              << ":\n"
                              << outputTail;
        }
    } else {
        done.status = OperationStatus::Failed;
        done.message = summarizeHostFlatpakOutput(collected);
        if (done.message.isEmpty()) {
            done.message = FlatpakTransactionRunner::tr("Host flatpak install failed (exit %1).")
                                   .arg(proc.exitCode());
        }
        logFlatpakFailure(
                done,
                QStringLiteral("%1 elapsedMs=%2 exitCode=%3")
                        .arg(done.message)
                        .arg(elapsedMs)
                        .arg(proc.exitCode()),
                outputTail);
    }

    QMetaObject::invokeMethod(
            runner,
            [runner, done]() { emit runner->operationFinished(done); },
            Qt::QueuedConnection);
}
#endif
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
    if (message.contains(QStringLiteral("apply_extra"), Qt::CaseInsensitive)
            || message.contains(QStringLiteral("apply extra"), Qt::CaseInsensitive)
            || message.contains(QStringLiteral("/proc/self/fd/"), Qt::CaseInsensitive)) {
        return FlatpakTransactionRunner::tr(
                "Install failed while unpacking extra app data (apply_extra). "
                "Try: flatpak install flathub <app-id>, or run Curio with CURIO_FLATPAK_DEBUG=1.");
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

void FlatpakTransactionRunner::executeTransaction(
        FlatpakInstallation *installation,
        const std::function<bool(FlatpakTransaction *, GError **)> &buildTransaction,
        TransactionContext context)
{
    QElapsedTimer timer;
    timer.start();

    struct Defer {
        FlatpakTransactionRunner *runner;
        ~Defer()
        {
            runner->m_activeTransactions.fetch_sub(1, std::memory_order_relaxed);
        }
    } defer{this};

    g_autoptr(GError) error = nullptr;
    g_autoptr(FlatpakTransaction) transaction =
            flatpak_transaction_new_for_installation(installation, nullptr, &error);
    if (!transaction) {
        Operation failed = context.op;
        failed.status = OperationStatus::Failed;
        failed.message = friendlyError(error);
        logFlatpakFailure(failed, failed.message);
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
        logFlatpakFailure(failed, failed.message);
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
    const qint64 elapsedMs = timer.elapsed();
    if (ok) {
        logFlatpakDebug("finished",
                        done,
                        QStringLiteral("ok elapsedMs=%1").arg(elapsedMs));
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
        const QString glibMessage =
                error && error->message ? QString::fromUtf8(error->message) : done.message;
        logFlatpakFailure(done,
                          QStringLiteral("%1 elapsedMs=%2").arg(glibMessage).arg(elapsedMs));
    }

    QMetaObject::invokeMethod(this, [this, done, cleanup = context.cleanup]() {
        emit operationFinished(done);
        if (cleanup)
            cleanup();
    }, Qt::QueuedConnection);
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
    m_activeTransactions.fetch_add(1, std::memory_order_relaxed);

    // Run on a worker thread: flatpak_transaction_run drives D-Bus I/O and must not
    // block the Qt GUI thread (that deadlocks installs inside the Flatpak sandbox).
    (void) QtConcurrent::run([this, installationRef, buildTransaction, context]() {
        executeTransaction(installationRef, buildTransaction, context);
    });
}

void FlatpakTransactionRunner::waitForIdle(std::chrono::milliseconds timeout)
{
    QDeadlineTimer deadline(timeout);
    while (m_activeTransactions.load(std::memory_order_relaxed) > 0) {
        if (deadline.hasExpired())
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

    logFlatpakDebug("install-start", op, ref);

#ifdef CURIO_SANDBOXED_LIBFLATPAK
    // Embedded libflatpak + host bwrap cannot run apply_extra (Spotify, etc.) because
    // ro-bind sources use /proc/self/fd/N that do not survive flatpak-spawn --host.
    m_activeTransactions.fetch_add(1, std::memory_order_relaxed);
    (void) QtConcurrent::run([this, op, remote, scope]() {
        struct Defer {
            FlatpakTransactionRunner *runner;
            ~Defer()
            {
                runner->m_activeTransactions.fetch_sub(1, std::memory_order_relaxed);
            }
        } defer{this};
        runHostFlatpakInstall(this, op, remote, scope);
    });
    return;
#endif

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
