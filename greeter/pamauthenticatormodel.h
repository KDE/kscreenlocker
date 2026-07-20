// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <QRangeModel>
#include <qqmlintegration.h>

#include "pamauthenticators.h"

class QQmlEngine;
class QJSEngine;
class PAMAuthenticatorDescriptor;

class PAMAuthenticatorModel : public QRangeModel
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(AuthenticatorModel)
public:
    [[nodiscard]] static PAMAuthenticatorModel *instance();
    [[nodiscard]] static PAMAuthenticatorModel *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Helpers for PamAuthenticators accessing our internals. Bit out of place here but convenient.
    void markDefunct(PamAuthenticators::Authenticator authenticator) const;
    [[nodiscard]] bool isFunctional(PamAuthenticators::Authenticator authenticator) const;

private:
    using Range = std::vector<std::shared_ptr<PAMAuthenticatorDescriptor>>;
    using TypeHash = QHash<PamAuthenticators::Authenticator, std::shared_ptr<PAMAuthenticatorDescriptor>>;
    explicit PAMAuthenticatorModel(const Range &range, QObject *parent = nullptr);
    TypeHash m_hash;
};
