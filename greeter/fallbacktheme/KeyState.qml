/*
    SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQml
import org.kde.plasma.plasma5support as P5Support

// Fallback theme should rely on as little modules as possible. This is why
// the component that uses P5Support lives in a separate file. It allows a
// Loader to fail safely without disrupting the rest of the app.
P5Support.DataSource {
    engine: "keystate"
    connectedSources: "Caps Lock"
}
