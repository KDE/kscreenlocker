/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Kevin Ottens <kevin.ottens@enioka.com>

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QtControls
import QtQuick.Layouts 1.15

QtControls.StackView {
    id: main
    Layout.fillHeight: true
    implicitHeight: 490
    Layout.fillWidth: true
    property string sourceFile
    signal configurationChanged
    signal configurationForceChanged
    onSourceFileChanged: {
        if (sourceFile) {
            const props = {}
            const wallpaperConfig = configDialog.wallpaperConfiguration
            for (const key in wallpaperConfig) {
                props["cfg_" + key] = wallpaperConfig[key]
            }

            const newItem = replace(sourceFile, props, QtControls.StackView.ReplaceTransition)

            wallpaperConfig.valueChanged.connect((key, value) => {
                if (newItem["cfg_" + key] !== undefined) {
                    newItem["cfg_" + key] = value
                }
            })

            const createSignalHandler = key => {
                return () => {
                    configDialog.wallpaperConfiguration[key] = newItem["cfg_" + key]
                    configurationChanged()
                }
            }

            for (const key in wallpaperConfig) {
                const changedSignal = newItem["cfg_" + key + "Changed"]
                if (changedSignal) {
                    changedSignal.connect(createSignalHandler(key))
                }
            }

            const configurationChangedSignal = newItem.configurationChanged
            if (configurationChangedSignal) {
                configurationChangedSignal.connect(main.configurationForceChanged) // BUG 438585
            }
        } else {
            replace(empty)
        }
    }

    Component {
        id: empty
        Item {}
    }
}
