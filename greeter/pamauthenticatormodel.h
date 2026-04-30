// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <QRangeModel>

class PAMAuthenticatorModel : public QRangeModel
{
    Q_OBJECT
public:
    explicit PAMAuthenticatorModel(QObject *parent = nullptr);
};
