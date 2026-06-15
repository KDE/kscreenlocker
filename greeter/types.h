/*
 *  SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <qqmlregistration.h>

#include "pamauthenticator.h"
#include "pamauthenticators.h"

struct PamAuthenticatorForeign {
    Q_GADGET
    QML_NAMED_ELEMENT(Authenticator)
    QML_UNCREATABLE("")
    QML_FOREIGN(PamAuthenticator)
};

struct PamAuthenticatorsForeign {
    Q_GADGET
    QML_NAMED_ELEMENT(Authenticators)
    QML_UNCREATABLE("")
    QML_FOREIGN(PamAuthenticators)
};
