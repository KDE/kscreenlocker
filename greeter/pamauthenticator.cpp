/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pamauthenticator.h"

#include <QElapsedTimer>
#include <QMetaMethod>
#include <QThread>

#include "pamworker.h"

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
    , m_thread(std::make_unique<PamWorker>(service, user))
    , d(m_thread.worker())
{
    connect(d, &PamWorker::busyChanged, this, &PamAuthenticator::setBusy);
    connect(d, &PamWorker::inPasswordDelayChanged, this, &PamAuthenticator::setInPasswordDelay);
    connect(d, &PamWorker::prompt, this, [this](const QString &msg) {
        m_prompt = msg;
        Q_EMIT prompt(msg);
    });
    connect(d, &PamWorker::promptForSecret, this, [this](const QString &msg) {
        m_promptForSecret = msg;
        Q_EMIT promptForSecret(msg);
    });
    connect(d, &PamWorker::infoMessage, this, [this](const QString &msg) {
        m_infoMessage = msg;
        Q_EMIT infoMessage(msg);
    });
    connect(d, &PamWorker::errorMessage, this, [this](const QString &msg) {
        m_errorMessage = msg;
        Q_EMIT errorMessage(msg);
    });

    connect(d, &PamWorker::inAuthenticateChanged, this, [this](bool isInAuthenticate) {
        m_inAuthentication = isInAuthenticate;
        Q_EMIT availableChanged();
    });
    connect(d, &PamWorker::unavailabilityChanged, this, [this](bool isUnavailable) {
        m_unavailable = isUnavailable;
        Q_EMIT availableChanged();
    });

    connect(d, &PamWorker::succeeded, this, [this]() {
        m_unlocked = true;
        Q_EMIT succeeded();
    });
    // Failed is not a persistent state. When a view provides authentication that will either result in failure or success,
    // failure simply means that the prompt is getting delayed.
    connect(d, &PamWorker::failed, this, &PamAuthenticator::failed);
    connect(d, &PamWorker::loginFailedDelayStarted, this, &PamAuthenticator::loginFailedDelayStarted);

    m_thread.start();

    QMetaObject::invokeMethod(d, [this]() {
        d->start();
    });
}

PamAuthenticator::~PamAuthenticator()
{
    // This is a special thread type that cleans up the worker before returning to us.
    m_thread.quit();
    m_thread.wait();
}

bool PamAuthenticator::isBusy() const
{
    return m_busy;
}

bool PamAuthenticator::isAvailable() const
{
    return m_inAuthentication && !m_unavailable;
}

[[nodiscard]] bool PamAuthenticator::isReallyAvailable() const
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
    QMetaObject::invokeMethod(d, &PamWorker::authenticate);
}

void PamAuthenticator::respond(const QByteArray &response)
{
    QMetaObject::invokeMethod(
        d,
        [this, response]() {
            Q_EMIT d->promptResponseReceived(response);
        },
        Qt::QueuedConnection);
}

void PamAuthenticator::cancel()
{
    m_prompt.clear();
    m_promptForSecret.clear();
    m_infoMessage.clear();
    m_errorMessage.clear();
    QMetaObject::invokeMethod(d, &PamWorker::cancelled);
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

PamAuthenticator::WorkerThread::WorkerThread(std::unique_ptr<PamWorker> &&worker, QObject *parent)
    : QThread(parent)
    , m_worker(std::move(worker))
{
    m_worker->moveToThread(this);
}

[[nodiscard]] PamWorker *PamAuthenticator::WorkerThread::worker() const
{
    return m_worker.get();
}

void PamAuthenticator::WorkerThread::run()
{
    QThread::run();
    m_worker.reset();
}
