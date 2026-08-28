// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include "pamauthenticators.h"

class PAMAuthenticatorDescriptor : public QObject
{
    Q_OBJECT

    //! Whether this authenticator may be used at all; enabled by distro + enabled by user
    Q_PROPERTY(bool enabled MEMBER m_enabled CONSTANT)

    //! The type of this authenticator (Regular, Fingerprint, ...)
    Q_PROPERTY(PamAuthenticators::Authenticator type MEMBER m_type CONSTANT)

    //! The name of the icon to present for this authenticator in the UI
    Q_PROPERTY(QString iconName MEMBER m_iconName CONSTANT)

    //! Whether this authenticator requires a password field to be shown in the UI
    Q_PROPERTY(bool passwordField MEMBER m_passwordField CONSTANT)

    /*!
        Whether this authenticator is expected to signal a prompt before being considered working.
        Practically this is limited to "interactive" authenticators (e.g. password),
        while non-interactive authenticators (e.g. fingerprint) are expected to work without prompting even.

        e.g. consider pam-u2f. It has a 'cue' option that may not be set, which means there will be no prompt but
        the authenticator will still be working
    */
    Q_PROPERTY(bool expectingPrompt MEMBER m_expectingPrompt CONSTANT)

    //! The tooltip to show for the authenticator in the UI
    Q_PROPERTY(QString tooltip MEMBER m_tooltip CONSTANT)

    /*!
        Whether the prompt should be shown to the user.

        This is mostly a hint on whether the prompt is expected to be more useful than just "Password:".
        Specifically this is by default off for the Regular authenticator and must be opted into by the user when
        their PAM stack requires more involved prompt strategies (such as multi-factor authentication where the password
        input would expect different types of inputs depending on which PAM module is currently prompting).
    */
    Q_PROPERTY(bool showPrompt MEMBER m_showPrompt CONSTANT)

    /*!
        Whether this authenticator is currently functional.
        An authenticator may become dysfunctional if the underlying PAM stack shows signs of severe breakage.

        All authenticators except for Regular may become dysfunctional.
    */
    Q_PROPERTY(bool functional MEMBER m_functional NOTIFY functionalChanged)
public:
    PAMAuthenticatorDescriptor() = default;
    explicit PAMAuthenticatorDescriptor(bool enabled,
                                        PamAuthenticators::Authenticator type,
                                        const QString &iconName,
                                        bool passwordField,
                                        bool expectingPrompt,
                                        const QString &tooltip,
                                        bool showPrompt,
                                        QObject *parent = nullptr);
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] PamAuthenticators::Authenticator type() const;
    [[nodiscard]] bool isFunctional() const;
    void setFunctional(bool functional);

Q_SIGNALS:
    void functionalChanged();

private:
    // Mind that we must be default-constructible for QRangeModel to work. Always initialize members!
    bool m_enabled = false;
    PamAuthenticators::Authenticator m_type = PamAuthenticators::Authenticator::Regular;
    QString m_iconName;
    bool m_passwordField = false;
    bool m_expectingPrompt = false;
    QString m_tooltip;
    bool m_showPrompt = false;
    bool m_functional = true; // always functional by default
};
