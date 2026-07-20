// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include "pamauthenticatordescriptor.h"

#include <QDebug>

PAMAuthenticatorDescriptor::PAMAuthenticatorDescriptor(bool enabled,
                                                       PamAuthenticators::Authenticator type,
                                                       const QString &iconName,
                                                       bool passwordField,
                                                       bool expectingPrompt,
                                                       const QString &tooltip,
                                                       QObject *parent)
    : QObject(parent)
    , m_enabled(enabled)
    , m_type(type)
    , m_iconName(iconName)
    , m_passwordField(passwordField)
    , m_expectingPrompt(expectingPrompt)
    , m_tooltip(tooltip)
{
}

[[nodiscard]] bool PAMAuthenticatorDescriptor::isEnabled() const
{
    return m_enabled;
}

[[nodiscard]] PamAuthenticators::Authenticator PAMAuthenticatorDescriptor::type() const
{
    return m_type;
}

[[nodiscard]] bool PAMAuthenticatorDescriptor::isFunctional() const
{
    return m_functional;
}

void PAMAuthenticatorDescriptor::setFunctional(bool functioning)
{
    if (m_functional != functioning) {
        m_functional = functioning;
        Q_EMIT functionalChanged();
    }
}
