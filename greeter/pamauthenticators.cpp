/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>
    SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QDebug>
#include <QMetaEnum>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include "kscreenlocker_greet_logging.h"
#include "pamauthenticator.h"
#include "pamauthenticators.h"

using namespace Qt::StringLiterals;

namespace
{
void discardAuthenticator(std::unique_ptr<PamAuthenticator> &&authenticator)
{
    std::ignore = QtConcurrent::run([authenticator = std::move(authenticator)] {
        // Delete the old authenticator in another thread to avoid blocking the UI if the pam module is a bit rude (fprint for example blocks)
        // It is already disconnected from us and shouldn't have any adverse effects anymore.
    });
}
}

struct PamAuthenticators::Private {
    std::unique_ptr<PamAuthenticator> m_activeAuthenticator = nullptr;
    std::unique_ptr<PamAuthenticator> m_fingerprintAuthenticator = nullptr;
    PamAuthenticator::NoninteractiveAuthenticatorTypes computedTypes = PamAuthenticator::NoninteractiveAuthenticatorType::None;
    AuthenticatorsState state = AuthenticatorsState::Idle;
    bool graceLocked = false;
    bool hadPrompt = false;

    void recomputeNoninteractiveAuthenticationTypes()
    {
        if (!m_fingerprintAuthenticator || !m_fingerprintAuthenticator->isAvailable()) {
            return;
        }
        computedTypes = PamAuthenticator::NoninteractiveAuthenticatorType::Fingerprint;
    }
};

PamAuthenticators::PamAuthenticators(const QString &loginName, QObject *parent)
    : QObject(parent)
    , m_loginName(loginName)
    , d(new Private)
{
    connect(this, &PamAuthenticators::authenticatorChanged, this, &PamAuthenticators::onAuthenticatorChanged);
    QMetaObject::invokeMethod(this, &PamAuthenticators::onAuthenticatorChanged, Qt::QueuedConnection);
}

PamAuthenticators::~PamAuthenticators()
{
}

PamAuthenticators::Authenticator PamAuthenticators::loadAuthenticatorType()
{
    auto authenticators = QMetaEnum::fromType<Authenticator>();
    auto group = KSharedConfig::openStateConfig(u"kscreenlockerrc"_s)->group(u"Greeter"_s);
    auto authenticatorKey = group.readEntry("Authenticator", authenticators.valueToKey(std::to_underlying(Authenticator::Regular)));
    auto authenticatorValue = authenticators.keysToValue(authenticatorKey.toUtf8().constData());
    return static_cast<Authenticator>(authenticatorValue);
}

void PamAuthenticators::saveAuthenticatorType(Authenticator authenticator)
{
    // This only ought to happen when successfully unlocked using this authenticator. Not when switching to it!
    auto authenticators = QMetaEnum::fromType<Authenticator>();
    auto group = KSharedConfig::openStateConfig(u"kscreenlockerrc"_s)->group(u"Greeter"_s);
    group.writeEntry("Authenticator", authenticators.valueToKey(std::to_underlying(authenticator)));
    group.sync();
}

bool PamAuthenticators::isUnlocked() const
{
    return d->m_activeAuthenticator->isUnlocked() || d->m_fingerprintAuthenticator->isUnlocked();
}

PamAuthenticators::AuthenticatorsState PamAuthenticators::state() const
{
    return d->state;
}

void PamAuthenticators::startAuthenticating()
{
    if (d->graceLocked) {
        return;
    }

    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: starting authenticators";
    if (d->m_activeAuthenticator) {
        d->m_activeAuthenticator->tryUnlock();
    }
    if (d->m_fingerprintAuthenticator) {
        d->m_fingerprintAuthenticator->tryUnlock();
    }
    setState(AuthenticatorsState::Authenticating);
}

void PamAuthenticators::stopAuthenticating()
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: stopping authenticators";
    if (d->m_activeAuthenticator) {
        d->m_activeAuthenticator->cancel();
    }
    if (d->m_fingerprintAuthenticator) {
        d->m_fingerprintAuthenticator->cancel();
    }
    setState(AuthenticatorsState::Idle);
}

void PamAuthenticators::setState(AuthenticatorsState state)
{
    if (d->state == state) {
        return;
    }
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: state changing from" << d->state << "to" << state;
    d->state = state;
    Q_EMIT stateChanged();
}

void PamAuthenticators::onAuthenticatorChanged()
{
    if (d->m_activeAuthenticator) {
        d->m_activeAuthenticator->disconnect();
        d->m_activeAuthenticator->cancel();
        discardAuthenticator(std::move(d->m_activeAuthenticator));
    }

    if (m_authenticator == Authenticator::Fingerprint && d->m_fingerprintAuthenticator) {
        d->m_fingerprintAuthenticator->disconnect();
        d->m_fingerprintAuthenticator->cancel();
        discardAuthenticator(std::move(d->m_fingerprintAuthenticator));
    } else if (!d->m_fingerprintAuthenticator) {
        d->m_fingerprintAuthenticator =
            std::make_unique<PamAuthenticator>(QStringLiteral(KSCREENLOCKER_PAM_FINGERPRINT_SERVICE), m_loginName, PamAuthenticator::Fingerprint);
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::succeeded, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Success from non-interactive authenticator" << qUtf8Printable(d->m_fingerprintAuthenticator->service());
            saveAuthenticatorType(m_authenticator);
            Q_EMIT succeeded();
        });
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::availableChanged, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Availability changed for non-interactive authenticator"
                                         << qUtf8Printable(d->m_fingerprintAuthenticator->service()) << d->m_fingerprintAuthenticator->isAvailable();
            d->recomputeNoninteractiveAuthenticationTypes();
            Q_EMIT authenticatorTypesChanged();
        });
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::failed, this, [this] {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Non-interactive authenticator" << qUtf8Printable(d->m_fingerprintAuthenticator->service()) << "failed";
            Q_EMIT failed(d->m_fingerprintAuthenticator->authenticatorType(), d->m_fingerprintAuthenticator.get());
        });
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::loginFailedDelayStarted, this, [this](const uint uSecDelay) noexcept -> void {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Delay started on login failure for non-interactive authenticator"
                                         << qUtf8Printable(d->m_fingerprintAuthenticator->service()) << "duration:" << uSecDelay;
            Q_EMIT loginFailedDelayStarted(d->m_fingerprintAuthenticator->authenticatorType(), d->m_fingerprintAuthenticator.get(), uSecDelay);
        });
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::infoMessage, this, [this]() {
            if (!d->hadPrompt) {
                d->hadPrompt = true;
                Q_EMIT hadPromptChanged();
            }
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Info message from non-interactive authenticator" << qUtf8Printable(d->m_fingerprintAuthenticator->service());
            Q_EMIT noninteractiveInfo(d->m_fingerprintAuthenticator->authenticatorType(), d->m_fingerprintAuthenticator.get());
        });
        connect(d->m_fingerprintAuthenticator.get(), &PamAuthenticator::errorMessage, this, [this]() {
            qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Error message from non-interactive authenticator " << qUtf8Printable(d->m_fingerprintAuthenticator->service());
            Q_EMIT noninteractiveError(d->m_fingerprintAuthenticator->authenticatorType(), d->m_fingerprintAuthenticator.get());
        });
    }

    d->m_activeAuthenticator = [this] {
        Q_ASSERT_X(!m_loginName.isEmpty(), Q_FUNC_INFO, "login name must be set before constructing authenticators");
        auto make_unique = [this](const QString &service, PamAuthenticator::NoninteractiveAuthenticatorType type) {
            return std::make_unique<PamAuthenticator>(service, m_loginName, type);
        };
        switch(m_authenticator) {
        case Authenticator::Regular:
            return make_unique(QStringLiteral(KSCREENLOCKER_PAM_SERVICE), PamAuthenticator::None);
        case Authenticator::Fingerprint:
            return make_unique(QStringLiteral(KSCREENLOCKER_PAM_FINGERPRINT_SERVICE), PamAuthenticator::Fingerprint);
        case Authenticator::Smartcard:
            return make_unique(QStringLiteral(KSCREENLOCKER_PAM_SMARTCARD_SERVICE), PamAuthenticator::Smartcard);
        case Authenticator::Face:
            return make_unique(QStringLiteral(KSCREENLOCKER_PAM_FACE_SERVICE), PamAuthenticator::Face);
        case Authenticator::Universal2Factor:
            return make_unique(QStringLiteral(KSCREENLOCKER_PAM_UNIVERSAL2FACTOR_SERVICE), PamAuthenticator::Universal2Factor);
        }
        Q_ASSERT_X(false, Q_FUNC_INFO, "unhandled authenticator type");
        return std::unique_ptr<PamAuthenticator>{};
    }();
    // Old one got deleted and thus disconnected. Let's connect the new one!

    auto authenticator = d->m_activeAuthenticator.get();
    connect(authenticator, &PamAuthenticator::succeeded, this, [this, authenticator] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Success from interactive authenticator" << qUtf8Printable(authenticator->service());
        saveAuthenticatorType(m_authenticator);
        Q_EMIT succeeded();
    });
    connect(authenticator, &PamAuthenticator::failed, this, [this, authenticator] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Failure from interactive authenticator" << qUtf8Printable(authenticator->service());
        setState(AuthenticatorsState::Idle);
        Q_EMIT failed(PamAuthenticator::NoninteractiveAuthenticatorType::None, authenticator);
    });
    connect(authenticator, &PamAuthenticator::loginFailedDelayStarted, this, [this, authenticator](const uint uSecDelay) noexcept -> void {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Delay started on login failure for interactive authenticator" << qUtf8Printable(authenticator->service())
        << "duration:" << uSecDelay;
        Q_EMIT loginFailedDelayStarted(PamAuthenticator::NoninteractiveAuthenticatorType::None, authenticator, uSecDelay);
    });
    connect(authenticator, &PamAuthenticator::busyChanged, this, [this, authenticator] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Interactive authenticator" << qUtf8Printable(authenticator->service()) << "changed business";
        Q_EMIT busyChanged();
    });
    connect(authenticator, &PamAuthenticator::prompt, this, [this, authenticator] {
        if (!d->hadPrompt) {
            d->hadPrompt = true;
            Q_EMIT hadPromptChanged();
        }
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Normal prompt from interactive authenticator" << qUtf8Printable(authenticator->service());
        Q_EMIT promptChanged();
    });
    connect(authenticator, &PamAuthenticator::promptForSecret, this, [this, authenticator] {
        if (!d->hadPrompt) {
            d->hadPrompt = true;
            Q_EMIT hadPromptChanged();
        }
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Secret prompt from interactive authenticator" << qUtf8Printable(authenticator->service());
        Q_EMIT promptForSecretChanged();
    });
    connect(authenticator, &PamAuthenticator::infoMessage, this, [this, authenticator] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Info message from interactive authenticator" << qUtf8Printable(authenticator->service());
        Q_EMIT infoMessageChanged();
    });
    connect(authenticator, &PamAuthenticator::errorMessage, this, [this, authenticator] {
        qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: Error message from interactive authenticator" << qUtf8Printable(authenticator->service());
        Q_EMIT errorMessageChanged();
    });

    d->graceLocked = false;
    // graceLock has no signal
    d->hadPrompt = false;
    Q_EMIT hadPromptChanged();
    d->state = AuthenticatorsState::Idle;
    Q_EMIT stateChanged();

    Q_EMIT promptChanged();
    Q_EMIT promptForSecretChanged();
    Q_EMIT infoMessageChanged();
    Q_EMIT errorMessageChanged();
    Q_EMIT busyChanged();

    startAuthenticating();
}


// these properties are delegated to interactive authenticator

bool PamAuthenticators::isBusy() const
{
    return d->m_activeAuthenticator->isBusy();
}

QString PamAuthenticators::prompt() const
{
    return d->m_activeAuthenticator->getPrompt();
}

QString PamAuthenticators::promptForSecret() const
{
    return d->m_activeAuthenticator->getPromptForSecret();
}

QString PamAuthenticators::infoMessage() const
{
    return d->m_activeAuthenticator->getInfoMessage();
}

QString PamAuthenticators::errorMessage() const
{
    return d->m_activeAuthenticator->getErrorMessage();
}

void PamAuthenticators::respond(const QByteArray &response)
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: responding to interactive authenticator";
    d->m_activeAuthenticator->respond(response);
}

void PamAuthenticators::cancel()
{
    qCDebug(KSCREENLOCKER_GREET) << "PamAuthenticators: cancelling interactive authenticator";
    d->m_activeAuthenticator->cancel();
}

PamAuthenticator::NoninteractiveAuthenticatorTypes PamAuthenticators::authenticatorTypes() const
{
    return d->computedTypes;
}

void PamAuthenticators::setGraceLocked(bool b)
{
    d->graceLocked = b;
}

bool PamAuthenticators::hadPrompt() const
{
    return d->hadPrompt;
}
