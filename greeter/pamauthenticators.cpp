/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pamauthenticator.h"
#include "pamauthenticators.h"

#include "kscreenlocker_greet_logging.h"

PamAuthenticators::PamAuthenticators(std::unique_ptr<PamAuthenticator> &&interactive,
                                     std::vector<std::unique_ptr<PamAuthenticator>> &&noninteractive,
                                     QObject *parent)
    : AbstractAuthenticators(parent)
    , m_interactive(std::move(interactive))
    , m_noninteractive(std::move(noninteractive))
{
    connect(m_interactive.get(), &PamAuthenticator::succeeded, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Success from interactive authenticator" << qUtf8Printable(m_interactive->service());
        Q_EMIT succeeded();
    });
    connect(m_interactive.get(), &PamAuthenticator::failed, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Failure from interactive authenticator" << qUtf8Printable(m_interactive->service());
        m_state = AuthenticatorsState::Failed;
        cancelNoninteractive();
        Q_EMIT failed(NoninteractiveAuthenticatorType::None, m_interactive.get());
    });

    for (auto &noninteractive : m_noninteractive) {
        connect(noninteractive.get(), &PamAuthenticator::succeeded, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Success from non-interactive authenticator"
                                         << qUtf8Printable(static_cast<PamAuthenticator *>(sender())->service());
            Q_EMIT succeeded();
        });
        connect(noninteractive.get(), &PamAuthenticator::availableChanged, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Availability changed for non-interactive authenticator"
                                         << qUtf8Printable(static_cast<PamAuthenticator *>(sender())->service())
                                         << static_cast<PamAuthenticator *>(sender())->isAvailable();
            recomputeNoninteractiveAuthenticationTypes();
        });
        connect(noninteractive.get(), &PamAuthenticator::failed, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Non-interactive authenticator"
                                         << qUtf8Printable(static_cast<PamAuthenticator *>(sender())->service()) << "failed";
            m_state = AuthenticatorsState::Failed;
            cancelNoninteractive();
            Q_EMIT failed(static_cast<PamAuthenticator *>(sender())->authenticatorType(), sender());
        });
        connect(noninteractive.get(), &PamAuthenticator::infoMessage, this, [this]() {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Info message from non-interactive authenticator"
                                         << qUtf8Printable(static_cast<PamAuthenticator *>(sender())->service());
            Q_EMIT noninteractiveInfo(static_cast<PamAuthenticator *>(sender())->authenticatorType(), sender());
        });
        connect(noninteractive.get(), &PamAuthenticator::errorMessage, this, [this]() {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Error message from non-interactive authenticator "
                                         << qUtf8Printable(static_cast<PamAuthenticator *>(sender())->service());
            Q_EMIT noninteractiveError(static_cast<PamAuthenticator *>(sender())->authenticatorType(), sender());
        });
    }

    // connect the delegated signals
    connect(m_interactive.get(), &PamAuthenticator::busyChanged, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Interactive authenticator" << qUtf8Printable(m_interactive->service()) << "changed business";
        Q_EMIT busyChanged();
    });
    connect(m_interactive.get(), &PamAuthenticator::prompt, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Normal prompt from interactive authenticator" << qUtf8Printable(m_interactive->service());
        Q_EMIT promptChanged();
    });
    connect(m_interactive.get(), &PamAuthenticator::promptForSecret, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Secret prompt from interactive authenticator" << qUtf8Printable(m_interactive->service());
        Q_EMIT promptForSecretChanged();
    });
    connect(m_interactive.get(), &PamAuthenticator::infoMessage, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Info message from interactive authenticator" << qUtf8Printable(m_interactive->service());
        Q_EMIT infoMessageChanged();
    });
    connect(m_interactive.get(), &PamAuthenticator::errorMessage, this, [this] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Error message from interactive authenticator" << qUtf8Printable(m_interactive->service());
        Q_EMIT errorMessageChanged();
    });
}

PamAuthenticators::~PamAuthenticators()
{
}

bool PamAuthenticators::isUnlocked() const
{
    return m_interactive->isUnlocked() || std::any_of(m_noninteractive.cbegin(), m_noninteractive.cend(), [](auto &t) {
               return t->isUnlocked();
           });
}

void PamAuthenticators::startAuthenticating()
{
    if (m_state == AuthenticatorsState::Authenticating) {
        return;
    }

    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: starting authenticators";
    m_interactive->tryUnlock();
    for (auto &noninteractive : m_noninteractive) {
        noninteractive->tryUnlock();
    }
    m_state = AuthenticatorsState::Authenticating;
}

void PamAuthenticators::stopAuthenticating()
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: stopping authenticators";
    for (auto &noninteractive : m_noninteractive) {
        noninteractive->cancel();
    }
    m_interactive->cancel();
    m_state = AuthenticatorsState::Idle;
}

// these properties are delegated to interactive authenticator

bool PamAuthenticators::isBusy() const
{
    return m_interactive->isBusy();
}

QString PamAuthenticators::prompt() const
{
    return m_interactive->getPrompt();
}

QString PamAuthenticators::promptForSecret() const
{
    return m_interactive->getPromptForSecret();
}

QString PamAuthenticators::infoMessage() const
{
    return m_interactive->getInfoMessage();
}

QString PamAuthenticators::errorMessage() const
{
    return m_interactive->getErrorMessage();
}

void PamAuthenticators::respond(const QByteArray &response)
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: responding to interactive authenticator";
    m_interactive->respond(response);
}

void PamAuthenticators::cancel()
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: cancelling interactive authenticator";
    m_interactive->cancel();
}

void PamAuthenticators::recomputeNoninteractiveAuthenticationTypes()
{
    NoninteractiveAuthenticatorType::Types result = NoninteractiveAuthenticatorType::None;
    for (auto &noninteractive : m_noninteractive) {
        if (noninteractive->isAvailable()) {
            result |= noninteractive->authenticatorType();
        }
    }
    m_computedTypes = result;
}

void PamAuthenticators::cancelNoninteractive()
{
    for (auto &noninteractive : m_noninteractive) {
        noninteractive->cancel();
    }
}

#include "moc_pamauthenticators.cpp"
