// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <QtGlobal>

#if !defined(Q_OS_FREEBSD)
#include <sys/prctl.h>
#else
#include <sys/procctl.h>
#endif

#include <cerrno>
#include <csignal>
#include <expected>

[[nodiscard]] inline std::expected<void, int> dieWithParent()
{
#if !defined(Q_OS_FREEBSD)
    // PR_SET_PDEATHSIG returns -1 on error and sets errno
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) == -1) {
        return std::unexpected{errno};
    }
    return {};
#else
    auto sig = SIGKILL;
    // procctl returns -1 on error and sets errno, it is undefined which value is returned on success
    if (procctl(P_PID, 0, PROC_PDEATHSIG_CTL, static_cast<void *>(&sig)) == -1) {
        return std::unexpected{errno};
    }
    return {};
#endif
}
