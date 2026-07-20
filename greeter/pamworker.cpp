// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include "pamworker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServer>
#include <QProcess>
#include <QTimer>

#include <KLibexec>

#include "kscreenlocker_greet_logging.h"
#include "result.h"
#include "screenlockeradaptor.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

PamWorker::PamWorker(const QString &service, const QString &user)
    : QObject(nullptr)
    , m_nextAttemptAllowedTime(std::chrono::steady_clock::now())
    , m_service(service)
    , m_username(user)
{
    connect(this, &PamWorker::promptResponseReceived, this, [this](const QByteArray &response) {
        if (m_pendingPrompt.type() != QDBusMessage::InvalidMessage) {
            QDBusMessage reply = m_pendingPrompt.createReply(QString::fromUtf8(response));
            m_dbusWorker->connection().send(reply);
            m_pendingPrompt = {};
        } else {
            qCWarning(KSCREENLOCKER_GREET) << "Received prompt response, but no pending prompt was found!";
        }
    });

    connect(this, &PamWorker::cancelled, this, [this] {
        if (!m_dbusWorker) {
            return;
        }
        // Timings here are tight because we may need to cancel two workers, we don't want to get stuck on this for too long.
        // Let it quit on its own. Mind that this usually will get stuck because the worker is itself waiting for a prompt response.
        std::ignore = m_dbusWorker->Cancel();
        if (m_workerProcess) {
            m_workerProcess->waitForFinished((5ms).count());
        }
        // Make sure everything is cleaned up properly
        quitWorkerProcess();
    });
}

PamWorker::~PamWorker()
{
    quitWorkerProcess();
}

void PamWorker::authenticate()
{
    if (m_inAuthenticate) {
        // No need to log in this case. We call authenticate multiple times to keep the auth session running even
        // when the backend (e.g. fprint) aborts things.
        return;
    }

    if (!m_dbusWorker) {
        qCWarning(KSCREENLOCKER_GREET) << "DBus worker not initialized, cannot authenticate yet. Retry in a bit.";
        return;
    }

    if (m_unavailable) {
        qCDebug(KSCREENLOCKER_GREET) << "PAM service is not available. Cannot authenticate.";
        return;
    }

    m_inAuthenticate = true;
    Q_EMIT inAuthenticateChanged(m_inAuthenticate);
    Q_EMIT busyChanged(true);
    auto scopedAuthenticate = qScopeGuard([this] {
        m_inAuthenticate = false;
        Q_EMIT inAuthenticateChanged(m_inAuthenticate);
        Q_EMIT busyChanged(false);
    });

    auto watcher = new QDBusPendingCallWatcher(m_dbusWorker->Authenticate(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [scopedAuthenticate = std::move(scopedAuthenticate), watcher, this]() {
        watcher->deleteLater();
        if (watcher->error().isValid()) {
            qCWarning(KSCREENLOCKER_GREET) << "PAM worker failed to authenticate:" << watcher->error().message();
            Q_EMIT failed();
            return;
        }

        decltype(m_dbusWorker->Authenticate()) reply = watcher->reply();
        switch (reply.value()) {
        case WorkerResult::Type::Failure:
            Q_EMIT failed();
            return;
        case WorkerResult::Type::Success:
            Q_EMIT succeeded();
            return;
        case WorkerResult::Type::Unavailable:
            m_unavailable = true;
            Q_EMIT unavailabilityChanged(m_unavailable);
            return;
        };

        qWarning() << "Unexpected authentication result" << reply.value();
    });
}

void PamWorker::StartFailedDelay(uint useconds)
{
    m_nextAttemptAllowedTime = std::chrono::steady_clock::now() + std::chrono::microseconds(useconds);
    Q_EMIT inPasswordDelayChanged(true);
    QTimer::singleShot(std::chrono::microseconds(useconds), this, [this]() {
        Q_EMIT inPasswordDelayChanged(false);
    });
    Q_EMIT loginFailedDelayStarted(useconds);
}

void PamWorker::InfoMessage(const QString &msg)
{
    Q_EMIT infoMessage(msg);
}

void PamWorker::ErrorMessage(const QString &msg)
{
    Q_EMIT errorMessage(msg);
}

void PamWorker::start()
{
    Q_ASSERT(!m_workerProcess);

    qCDebug(KSCREENLOCKER_GREET) << "Starting PAM worker for service" << m_service << "and user" << m_username;
    new ScreenlockerAdaptor(this);

    auto server = new QDBusServer(this);
    QObject::connect(server, &QDBusServer::newConnection, this, [this](const QDBusConnection &connection) {
        Q_ASSERT(!m_dbusWorker); // only accept a connection once to mitigate the risk of a malicious actor connecting to us
        qCDebug(KSCREENLOCKER_GREET) << "New D-Bus connection established" << connection.name();
        auto c = connection; // make a non-const copy
        c.registerObject(u"/org/kde/plasma/screenlocker"_s, this, QDBusConnection::ExportAdaptors);
        m_dbusWorker = std::make_unique<org::kde::plasma::screenlocker::worker>(QString(), u"/org/kde/plasma/screenlocker/worker"_s, c);
        m_dbusWorker->setTimeout(std::numeric_limits<int>::max()); // disable timeout, we expect blocking calls to arrive eventually
    });

    m_workerProcess = new QProcess(this);
    m_workerProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_workerProcess->setProgram(KLibexec::path(u"kscreenlocker_worker"_s));
    m_workerProcess->setArguments({m_service});
    m_workerProcess->start();
    m_workerProcess->write(server->address().toUtf8());
    m_workerProcess->closeWriteChannel();
}

void PamWorker::Ping(const QString &message)
{
    qCDebug(KSCREENLOCKER_GREET) << "Ping received" << message << calledFromDBus();
    org::kde::plasma::screenlocker::worker worker(QString(), u"/org/kde/plasma/screenlocker/worker"_s, connection());
    auto watcher = new QDBusPendingCallWatcher(worker.Start(m_service, m_username), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher]() {
        watcher->deleteLater();
        Q_ASSERT(watcher->isValid());
    });
}

QString PamWorker::Prompt(const QString &msg)
{
    return promptInternal(msg, false);
}

QString PamWorker::MaskedPrompt(const QString &msg)
{
    return promptInternal(msg, true);
}

QString PamWorker::promptInternal(const QString &msg, bool isSecret)
{
    Q_EMIT busyChanged(false);

    if (isSecret) {
        Q_EMIT promptForSecret(msg);
    } else {
        Q_EMIT prompt(msg);
    }

    qCDebug(KSCREENLOCKER_GREET,
            "[PAM worker %s] Message: %s: %s",
            qUtf8Printable(m_service),
            (isSecret ? "Echo-off prompt" : "Echo-on prompt"),
            qUtf8Printable(msg));

    setDelayedReply(true);

    m_pendingPrompt = message();

    return {};
}

void PamWorker::quitWorkerProcess()
{
    if (m_workerProcess) {
        m_workerProcess->terminate();
        if (!m_workerProcess->waitForFinished((25ms).count())) {
            qWarning() << "Worker did not terminate in time, killing it.";
            m_workerProcess->kill();
        }
    }
    m_dbusWorker.reset();
    m_workerProcess = nullptr;
}
