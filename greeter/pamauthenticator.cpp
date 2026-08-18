/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pamauthenticator.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusServer>
#include <QMetaMethod>
#include <QProcess>
#include <QTimer>

#include <KLibexec>

#include "kscreenlocker_greet_logging.h"
#include "org.kde.plasma.screenlocker.worker.h"
#include "result.h"
#include "screenlockeradaptor.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

PamAuthenticator::PamAuthenticator(const QString &service, const QString &user, NoninteractiveAuthenticatorTypes types, QObject *parent)
    : QObject(parent)
    , m_signalsToMembers({
          {QMetaMethod::fromSignal(&PamAuthenticator::prompt), m_prompt},
          {QMetaMethod::fromSignal(&PamAuthenticator::promptForSecret), m_promptForSecret},
          {QMetaMethod::fromSignal(&PamAuthenticator::infoMessage), m_infoMessage},
          {QMetaMethod::fromSignal(&PamAuthenticator::errorMessage), m_errorMessage},
      })
    , m_service(service)
    , m_authenticatorType(types)
    , m_user(user)
{
    new ScreenlockerAdaptor(this);
    startWorker();
}

PamAuthenticator::~PamAuthenticator()
{
    quitWorkerProcess();
}

bool PamAuthenticator::isBusy() const
{
    return m_busy;
}

[[nodiscard]] bool PamAuthenticator::isAvailable() const
{
    return !m_unavailable;
}

PamAuthenticator::NoninteractiveAuthenticatorTypes PamAuthenticator::authenticatorType() const
{
    return m_authenticatorType;
}

void PamAuthenticator::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        Q_EMIT busyChanged();
    }
}

bool PamAuthenticator::isUnlocked() const
{
    return m_unlocked;
}

void PamAuthenticator::tryUnlock()
{
    m_unlocked = false;

    if (m_busy) {
        return;
    }

    if (!m_dbusWorker) {
        qCWarning(KSCREENLOCKER_GREET) << "DBus worker not initialized, cannot authenticate yet.";
        return;
    }

    if (m_unavailable) {
        qCDebug(KSCREENLOCKER_GREET) << "PAM service is not available. Cannot authenticate.";
        return;
    }

    setBusy(true);
    auto watcher = new QDBusPendingCallWatcher(m_dbusWorker->Authenticate(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        watcher->deleteLater();
        setBusy(false);

        if (watcher->error().isValid()) {
            qCWarning(KSCREENLOCKER_GREET) << "PAM worker failed to authenticate:" << watcher->error().message();
            Q_EMIT failed();
            return;
        }

        decltype(m_dbusWorker->Authenticate()) reply = watcher->reply();
        switch (reply.value()) {
        case WorkerResult::Type::Failure: {
            // Guard against particularly broken PAM services. For example when pam-u2f doesn't find a token because it is
            // not plugged in it will fail the authentication, but it will do it so slowly that the timing checks in the
            // worker itself don't bite.
            // Here we can keep a higher level view of the failures and if need be break the loop by marking us unavailable.
            auto now = QDateTime::currentDateTimeUtc();
            if (now - m_lastFailed < 2s) {
                m_failedCount++;
                if (m_failedCount > 3) {
                    m_unavailable = true;
                    Q_EMIT availableChanged();
                }
            } else {
                m_failedCount = 0;
            }
            m_lastFailed = now;

            Q_EMIT failed();
            return;
        }
        case WorkerResult::Type::Success:
            m_unlocked = true;
            Q_EMIT succeeded();
            return;
        case WorkerResult::Type::Unavailable:
            m_unavailable = true;
            Q_EMIT availableChanged();
            return;
        };

        qWarning() << "Unexpected authentication result" << reply.value();
    });
}

void PamAuthenticator::respond(const QByteArray &response)
{
    if (m_pendingPrompt.type() != QDBusMessage::InvalidMessage && m_dbusWorker) {
        QDBusMessage reply = m_pendingPrompt.createReply(QString::fromUtf8(response));
        m_dbusWorker->connection().send(reply);
        m_pendingPrompt = {};
    } else {
        qCWarning(KSCREENLOCKER_GREET) << "Received prompt response, but no pending prompt was found!";
    }
}

void PamAuthenticator::cancel()
{
    m_prompt.clear();
    m_promptForSecret.clear();
    m_infoMessage.clear();
    m_errorMessage.clear();

    if (!m_dbusWorker) {
        return;
    }

    // Timings here are tight because we may need to cancel two workers, we don't want to get stuck on this for too long.
    // Let it quit on its own. Mind that this usually will get stuck because the worker is itself waiting for a prompt response.
    std::ignore = m_dbusWorker->Cancel();
    if (m_workerProcess) {
        m_workerProcess->waitForFinished((5ms).count());
    }
    quitWorkerProcess();
}

QString PamAuthenticator::getPrompt() const
{
    return m_prompt;
}

QString PamAuthenticator::getPromptForSecret() const
{
    return m_promptForSecret;
}

QString PamAuthenticator::getInfoMessage() const
{
    return m_infoMessage;
}

QString PamAuthenticator::getErrorMessage() const
{
    return m_errorMessage;
}

QString PamAuthenticator::service() const
{
    return m_service;
}

bool PamAuthenticator::inPasswordDelay() const
{
    return m_inPasswordDelay;
}

void PamAuthenticator::setInPasswordDelay(bool timeout)
{
    m_inPasswordDelay = timeout;
    Q_EMIT inPasswordDelayChanged();
}

void PamAuthenticator::Ping(const QString &message)
{
    qCDebug(KSCREENLOCKER_GREET) << "Ping received" << message << calledFromDBus();
    if (!m_dbusWorker) {
        qCWarning(KSCREENLOCKER_GREET) << "Worker pinged before DBus worker interface was initialized.";
        return;
    }

    auto watcher = new QDBusPendingCallWatcher(m_dbusWorker->Start(m_service, m_user), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher]() {
        watcher->deleteLater();
        Q_ASSERT(watcher->isValid());
    });

    Q_ASSERT(!m_ready); // only one ping ever. thank you!
    m_ready = true;
    Q_EMIT readyChanged();
}

QString PamAuthenticator::Prompt(const QString &msg)
{
    return promptInternal(msg, false);
}

QString PamAuthenticator::MaskedPrompt(const QString &msg)
{
    return promptInternal(msg, true);
}

void PamAuthenticator::StartFailedDelay(uint useconds)
{
    setInPasswordDelay(true);
    QTimer::singleShot(std::chrono::microseconds(useconds), this, [this]() {
        setInPasswordDelay(false);
    });
    Q_EMIT loginFailedDelayStarted(useconds);
}

void PamAuthenticator::InfoMessage(const QString &msg)
{
    m_infoMessage = msg;
    Q_EMIT infoMessage(msg);
}

void PamAuthenticator::ErrorMessage(const QString &msg)
{
    m_errorMessage = msg;
    Q_EMIT errorMessage(msg);
}

QString PamAuthenticator::promptInternal(const QString &msg, bool isSecret)
{
    setBusy(false);

    if (isSecret) {
        m_promptForSecret = msg;
        Q_EMIT promptForSecret(msg);
    } else {
        m_prompt = msg;
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

void PamAuthenticator::quitWorkerProcess()
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

void PamAuthenticator::startWorker()
{
    Q_ASSERT(!m_workerProcess);

    qCDebug(KSCREENLOCKER_GREET) << "Starting PAM worker for service" << m_service << "and user" << m_user;

    m_server = new QDBusServer(this);
    connect(m_server, &QDBusServer::newConnection, this, &PamAuthenticator::connectWorker);

    m_workerProcess = new QProcess(this);
    m_workerProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_workerProcess->setProgram(KLibexec::path(u"kscreenlocker_worker"_s));
    m_workerProcess->setArguments({m_service});
    m_workerProcess->start();
    m_workerProcess->write(m_server->address().toUtf8());
    m_workerProcess->closeWriteChannel();
}

void PamAuthenticator::connectWorker(const QDBusConnection &connection)
{
    Q_ASSERT(!m_dbusWorker); // only accept a connection once to mitigate the risk of a malicious actor connecting to us
    qCDebug(KSCREENLOCKER_GREET) << "New D-Bus connection established" << connection.name();
    auto c = connection; // make a non-const copy
    c.registerObject(u"/org/kde/plasma/screenlocker"_s, this, QDBusConnection::ExportAdaptors);
    m_dbusWorker = std::make_unique<OrgKdePlasmaScreenlockerWorkerInterface>(QString(), u"/org/kde/plasma/screenlocker/worker"_s, c);
    m_dbusWorker->setTimeout(std::numeric_limits<int>::max()); // disable timeout, we expect blocking calls to arrive eventually
}
