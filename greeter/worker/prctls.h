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

namespace PRCTLs
{

[[nodiscard]] inline std::expected<void, int> _expecting(int ret)
{
    // Linux: returns -1 on error and sets errno
    // FreeBSD: returns -1 on error and sets errno, it is undefined which value is returned on success
    if (ret == -1) {
        return std::unexpected{errno};
    }
    return {};
}

[[nodiscard]] inline auto dieWithParent()
{
#if !defined(Q_OS_FREEBSD)
    return _expecting(prctl(PR_SET_PDEATHSIG, SIGKILL));
#else
    auto sig = SIGKILL;
    return _expecting(procctl(P_PID, 0, PROC_PDEATHSIG_CTL, static_cast<void *>(&sig)));
#endif
}

[[nodiscard]] inline auto setDumpable(bool dumpable)
{
#if !defined(Q_OS_FREEBSD)
    return _expecting(prctl(PR_SET_DUMPABLE, dumpable ? 1 : 0));
#else
    auto mode = dumpable ? PROC_TRACE_CTL_ENABLE : PROC_TRACE_CTL_DISABLE;
    return _expecting(procctl(P_PID, 0, PROC_TRACE_CTL, &mode));
#endif
}

} // namespace PRCTLs
