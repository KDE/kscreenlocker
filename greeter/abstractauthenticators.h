/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QBindable>
#include <QObject>
#ifdef BUILD_DECLARATIVE
#include <qqmlregistration.h>
#endif

#include "authenticatorsstate.h"
#include "noninteractiveauthenticatortype.h"

class AbstractAuthenticators : public QObject
{
    Q_OBJECT
#ifdef BUILD_DECLARATIVE
    QML_ELEMENT
#endif

    Q_PROPERTY(QString promptForSecret READ promptForSecret NOTIFY promptForSecretChanged)

    // this property is a sum of the noninteractive authenticators' flags
    Q_PROPERTY(NoninteractiveAuthenticatorType::Types authenticatorTypes READ authenticatorTypes NOTIFY authenticatorTypesChanged)

    Q_PROPERTY(AuthenticatorsState::State state READ state NOTIFY stateChanged)

public:
    using QObject::QObject;

    virtual QString promptForSecret() const;

    AuthenticatorsState::State state() const;
    Q_INVOKABLE virtual void startAuthenticating();
    Q_INVOKABLE virtual void stopAuthenticating();

    // these delegate to interactive authenticator
    Q_INVOKABLE virtual void respond(const QByteArray &response);
    Q_INVOKABLE virtual void cancel();

    NoninteractiveAuthenticatorType::Types authenticatorTypes() const;

Q_SIGNALS:
    void failed(NoninteractiveAuthenticatorType::Types what, QObject *authenticator);
    void succeeded();

    void authenticatorTypesChanged();
    void promptForSecretChanged();
    void stateChanged();

protected:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractAuthenticators,
                                         AuthenticatorsState::State,
                                         m_state,
                                         AuthenticatorsState::Idle,
                                         &AbstractAuthenticators::stateChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractAuthenticators,
                                         NoninteractiveAuthenticatorType::Types,
                                         m_computedTypes,
                                         NoninteractiveAuthenticatorType::None,
                                         &AbstractAuthenticators::authenticatorTypesChanged)
};
