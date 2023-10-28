/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "pamauthenticator.h"
#include <QObject>
#include <memory>

#include "abstractauthenticators.h"

class PamAuthenticators : public AbstractAuthenticators
{
    Q_OBJECT

    // these properties delegate to the interactive authenticator
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

    Q_PROPERTY(QString prompt READ prompt NOTIFY promptChanged)

    Q_PROPERTY(QString infoMessage READ infoMessage NOTIFY infoMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

    // this property true if any of the authenticators' unlocked properties are true
    Q_PROPERTY(bool unlocked READ isUnlocked NOTIFY succeeded)

public:
    explicit PamAuthenticators(std::unique_ptr<PamAuthenticator> &&interactive,
                               std::vector<std::unique_ptr<PamAuthenticator>> &&noninteractive,
                               QObject *parent = nullptr);
    ~PamAuthenticators() override;

    Q_INVOKABLE void startAuthenticating() override;
    Q_INVOKABLE void stopAuthenticating() override;

    // these properties delegate to the interactive authenticator
    bool isBusy() const;
    Q_SIGNAL void busyChanged();

    QString prompt() const;
    Q_SIGNAL void promptChanged();
    QString promptForSecret() const override;
    QString infoMessage() const;
    Q_SIGNAL void infoMessageChanged();
    QString errorMessage() const;
    Q_SIGNAL void errorMessageChanged();

    // these delegate to interactive authenticator
    Q_INVOKABLE void respond(const QByteArray &response) override;
    Q_INVOKABLE void cancel() override;

    // this property is true if any of the authenticators' unlocked properties are true
    bool isUnlocked() const;

    Q_SIGNAL void noninteractiveError(NoninteractiveAuthenticatorType::Types what, QObject *authenticator);
    Q_SIGNAL void noninteractiveInfo(NoninteractiveAuthenticatorType::Types what, QObject *authenticator);

private:
    void recomputeNoninteractiveAuthenticationTypes();
    void cancelNoninteractive();

    std::unique_ptr<PamAuthenticator> m_interactive;
    std::vector<std::unique_ptr<PamAuthenticator>> m_noninteractive;
};
