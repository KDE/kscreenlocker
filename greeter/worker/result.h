// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

namespace WorkerResult
{
enum Type { // auto-conversion from int doesn't want to work on the greeter side of things, consequently we are using an old school enum here and coerce it to
            // int
    Failure = 0,
    Success = 1,
    Unavailable = 2,
};
} // namespace WorkerResult
