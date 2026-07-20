// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <memory>

#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>

#include "org.kde.plasma.screenlocker.worker.h"

class QProcess;

class PamWorker : public QObject, QDBusContext
{
    Q_OBJECT
public:
    PamWorker(const QString &service, const QString &user);
    ~PamWorker() override;
    Q_DISABLE_COPY_MOVE(PamWorker)
    void start();
    void authenticate();
    void StartFailedDelay(uint useconds);
    void InfoMessage(const QString &msg);
    void ErrorMessage(const QString &msg);

public Q_SLOTS:
    void Ping(const QString &message);
    [[nodiscard]] QString Prompt(const QString &msg);
    [[nodiscard]] QString MaskedPrompt(const QString &msg);

Q_SIGNALS:
    void busyChanged(bool busy);
    void promptForSecret(const QString &msg);
    void prompt(const QString &msg);
    void infoMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void failed();
    void loginFailedDelayStarted(const uint uSecDelay);
    void succeeded();
    void unavailabilityChanged(bool unavailable);
    void inAuthenticateChanged(bool inAuthenticate);
    void inPasswordDelayChanged(bool timeout);

    // internal
    void promptResponseReceived(const QByteArray &prompt);
    void cancelled();

private:
    [[nodiscard]] QString promptInternal(const QString &msg, bool isSecret);
    void quitWorkerProcess();

    bool m_unavailable = false;
    bool m_inAuthenticate = false;
    std::chrono::steady_clock::time_point m_nextAttemptAllowedTime;
    int m_result = -1;
    QString m_service;
    QString m_username;
    QDBusMessage m_pendingPrompt;
    std::unique_ptr<org::kde::plasma::screenlocker::worker> m_dbusWorker;
    QProcess *m_workerProcess = nullptr;
};
