/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QFlags>
#include <qnamespace.h>
#ifdef BUILD_DECLARATIVE
#include <qqmlregistration.h>
#endif

namespace NoninteractiveAuthenticatorType
{
#ifdef BUILD_DECLARATIVE
QML_ELEMENT
#endif
Q_NAMESPACE
enum Type {
    None = 0,
    Fingerprint = 1 << 0,
    Smartcard = 1 << 1,
};
Q_DECLARE_FLAGS(Types, Type)
Q_FLAG_NS(Types)
Q_DECLARE_OPERATORS_FOR_FLAGS(Types)
}
