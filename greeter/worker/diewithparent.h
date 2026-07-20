// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <QtGlobal>

#if defined(Q_OS_FREEBSD)
#include <sys/procctl.h>
#else
#include <sys/prctl.h>
#endif

#include <csignal>

#if defined(Q_OS_FREEBSD)
inline auto dieWithParent()
{
    auto sig = SIGKILL;
    return procctl(P_PID, 0, PROC_PDEATHSIG_CTL, static_cast<void *>(&sig));
}
#else
inline auto dieWithParent()
{
    return prctl(PR_SET_PDEATHSIG, SIGKILL);
}
#endif
