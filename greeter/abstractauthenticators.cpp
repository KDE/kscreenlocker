/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "abstractauthenticators.h"

using namespace Qt::StringLiterals;

AuthenticatorsState::State AbstractAuthenticators::state() const
{
    return m_state;
}

QString AbstractAuthenticators::promptForSecret() const
{
    return m_state == AuthenticatorsState::Authenticating ? u"Authenticating"_s : QString();
}

void AbstractAuthenticators::startAuthenticating()
{
#ifdef BUILD_DECLARATIVE
    if (m_state == AuthenticatorsState::Authenticating) {
        return;
    }
    m_state = AuthenticatorsState::Authenticating;
    Q_EMIT promptForSecretChanged();
#endif
}

void AbstractAuthenticators::stopAuthenticating()
{
#ifdef BUILD_DECLARATIVE
    m_state = AuthenticatorsState::Idle;
#endif
}

void AbstractAuthenticators::respond(const QByteArray &response)
{
#ifdef BUILD_DECLARATIVE
    if (m_state != AuthenticatorsState::Authenticating) {
        return;
    }
    if (response == "myPassword") {
        m_state = AuthenticatorsState::Idle;
        Q_EMIT succeeded();
    } else {
        m_state = AuthenticatorsState::Failed;
        Q_EMIT failed(NoninteractiveAuthenticatorType::None, nullptr);
    }
#endif
}

void AbstractAuthenticators::cancel()
{
#ifdef BUILD_DECLARATIVE
    m_state = AuthenticatorsState::Idle;
#endif
}

NoninteractiveAuthenticatorType::Types AbstractAuthenticators::authenticatorTypes() const
{
    return m_computedTypes;
}

#include "moc_abstractauthenticators.cpp"
