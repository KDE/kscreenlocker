/*
    SPDX-FileCopyrightText: 2023 Janet Blackquill <uhhadd@gmail.com>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <qnamespace.h>
#ifdef BUILD_DECLARATIVE
#include <qqmlregistration.h>
#endif

namespace AuthenticatorsState
{
#ifdef BUILD_DECLARATIVE
QML_ELEMENT
#endif
Q_NAMESPACE
enum State {
    Idle,
    Authenticating,
    Failed,
};
Q_ENUM_NS(State)
}
