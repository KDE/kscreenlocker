/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QDateTime>
#include <QObject>
#include <QThread>
#include <qqmlregistration.h>

class PamWorker;

class PamAuthenticator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Authenticator)
    QML_UNCREATABLE("Not exposed except for its enum")

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(bool inPasswordDelay READ inPasswordDelay NOTIFY inPasswordDelayChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(NoninteractiveAuthenticatorTypes authenticatorType READ authenticatorType CONSTANT)

    Q_PROPERTY(QString prompt READ getPrompt NOTIFY prompt)
    Q_PROPERTY(QString promptForSecret READ getPromptForSecret NOTIFY promptForSecret)

    Q_PROPERTY(QString infoMessage READ getInfoMessage NOTIFY infoMessage)
    Q_PROPERTY(QString errorMessage READ getErrorMessage NOTIFY errorMessage)

    Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY succeeded)

public:
    /*!
        Exists purely to not have to change the UI code as of Plasma 6.8. Could eventually be dropped in favor of the
        PamAuthenticators::Authenticator enum.
     */
    enum NoninteractiveAuthenticatorType {
        None = 0,
        Fingerprint = 1 << 0,
        Smartcard = 2 << 0,
        Face = 3 << 0,
        Universal2Factor = 4 << 0,
    };
    Q_DECLARE_FLAGS(NoninteractiveAuthenticatorTypes, NoninteractiveAuthenticatorType)
    Q_FLAG(NoninteractiveAuthenticatorTypes)

    PamAuthenticator(const QString &service,
                     const QString &user,
                     NoninteractiveAuthenticatorTypes authenticatorType = NoninteractiveAuthenticatorType::None,
                     QObject *parent = nullptr);
    ~PamAuthenticator() override;
    Q_DISABLE_COPY_MOVE(PamAuthenticator)

    /*!
        Whether the Authenticator is currently doing something. This mostly means it is currently authenticating.
        A busy Authenticator probably won't be able to act on further tryUnlock calls.
     */
    bool isBusy() const;

    /*!
        Whether the Authenticator has successfully completed tryUnlock. i.e. the PAM service actually unlocked the account
     */
    bool isUnlocked() const;

    /*!
        Is this Authenticator actually available. An Authenticator goes unavailable when the underlying PAM service
        malfunctions and terminates too quickly, or when it reports itself PAM_AUTHINFO_UNAVAIL.
     */
    [[nodiscard]] bool isAvailable() const;

    NoninteractiveAuthenticatorTypes authenticatorType() const;

    // Get prefix to de-duplicate from their signals.
    QString getPrompt() const;
    QString getPromptForSecret() const;
    QString getInfoMessage() const;
    QString getErrorMessage() const;

    QString service() const;

    bool inPasswordDelay() const;

    void setInPasswordDelay(bool timeout);

Q_SIGNALS:
    void busyChanged();
    void promptForSecret(const QString &msg);
    void prompt(const QString &msg);
    void infoMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void succeeded();
    void failed();
    void availableChanged();
    void loginFailedDelayStarted(const uint uSecDelay);
    void inPasswordDelayChanged();

public Q_SLOTS:
    void tryUnlock();
    void respond(const QByteArray &response);
    void cancel();

private:
    void setBusy(bool busy);

    const std::vector<std::pair<QMetaMethod, const QString &>> m_signalsToMembers;
    // NOTE Don't forget to reset in cancel as necessary
    QString m_prompt;
    QString m_promptForSecret;
    QString m_errorMessage;
    QString m_infoMessage;
    QString m_service;
    bool m_busy = false;
    bool m_unlocked = false;
    bool m_unavailable = false;
    bool m_inPasswordDelay = false;
    NoninteractiveAuthenticatorTypes m_authenticatorType;
    // Tiny problem with bare bones QThread: when we shut down we want to clean up
    // our subprocess correctly, but doing that means running a function on the thread
    // before terminating it. This doesn't work out of the box because ther are
    // no facilities to effectively invokeMethod while the QApplication is already
    // mid shutdown. Instead we have a custom thread type that calls cleanup on the worker.
    class WorkerThread : public QThread
    {
    public:
        WorkerThread(std::unique_ptr<PamWorker> &&worker, QObject *parent = nullptr);
        [[nodiscard]] PamWorker *worker() const;
        void run() override;

    private:
        std::unique_ptr<PamWorker> m_worker;
    } m_thread;
    PamWorker *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PamAuthenticator::NoninteractiveAuthenticatorTypes)
