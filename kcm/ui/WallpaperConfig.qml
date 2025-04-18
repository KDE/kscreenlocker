/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Kevin Ottens <kevin.ottens@enioka.com>

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.kirigami 2.20 as Kirigami

QQC2.StackView {
    id: main

    property string sourceFile

    signal configurationChanged
    signal configurationForceChanged

    Layout.fillHeight: true
    Layout.fillWidth: true

    implicitHeight: main.empty ? 0 : (currentItem?.implicitHeight ?? 0)

    onSourceFileChanged: {
        if (sourceFile) {
            const wallpaperConfig = configDialog.wallpaperConfiguration
            const props = {
                "configDialog": configDialog,
                "wallpaperConfiguration": wallpaperConfig
            }

            // Some third-party wallpaper plugins need the config keys to be set initially.
            // We should not break them within one Plasma major version, but setting everything
            // will lead to an error message for every unused property (and some, like KConfigXT
            // default values, are used by almost no plugin configuration). We load the config
            // page in a temp variable first, then use that to figure out which ones we need to
            // set initially.
            // TODO Plasma 7: consider whether we can drop this workaround
            const temp = Qt.createComponent(Qt.resolvedUrl(sourceFile)).createObject(appearanceRoot, props)
            wallpaperConfig.keys().forEach(key => {
                const cfgKey = "cfg_" + key;
                if (cfgKey in temp) {
                    props[cfgKey] = wallpaperConfig[key]
                }
            })
            temp.destroy()

            const newItem = replace(sourceFile, props, QQC2.StackView.ReplaceTransition)

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
