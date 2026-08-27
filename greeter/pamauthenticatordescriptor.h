// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include "pamauthenticators.h"

class PAMAuthenticatorDescriptor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled MEMBER m_enabled CONSTANT)
    Q_PROPERTY(PamAuthenticators::Authenticator type MEMBER m_type CONSTANT)
    Q_PROPERTY(QString iconName MEMBER m_iconName CONSTANT)
    Q_PROPERTY(bool passwordField MEMBER m_passwordField CONSTANT)
    Q_PROPERTY(bool expectingPrompt MEMBER m_expectingPrompt CONSTANT)
    Q_PROPERTY(QString tooltip MEMBER m_tooltip CONSTANT)
    Q_PROPERTY(bool showPrompt MEMBER m_showPrompt CONSTANT)
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
    bool m_enabled;
    PamAuthenticators::Authenticator m_type;
    QString m_iconName;
    bool m_passwordField;
    bool m_expectingPrompt;
    QString m_tooltip;
    bool m_showPrompt;
    bool m_functional = true; // always functional by default
};
