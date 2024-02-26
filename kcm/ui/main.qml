/*
SPDX-FileCopyrightText: 2020 David Redondo <kde@david-redondo.de>

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.kcmutils as KCM
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kquickcontrols 2.0 as KQuickControls

KCM.SimpleKCM {
    id: root

    implicitHeight: Kirigami.Units.gridUnit * 45
    implicitWidth: Kirigami.Units.gridUnit * 45

    /// The options for the timeout before the screen locks.
    property var timeoutOptions: [
        { index: 0, text: i18nc("Screen will not lock automatically", "Never"), value: 0 },
        { index: 1, text: i18n("1 minute"), value: 1 },
        { index: 2, text: i18n("2 minutes"), value: 2 },
        { index: 3, text: i18n("5 minutes"), value: 5 },
        { index: 4, text: i18n("10 minutes"), value: 10 },
        { index: 5, text: i18n("15 minutes"), value: 15 },
        { index: 6, text: i18n("30 minutes"), value: 30 },
        { index: 7, text: i18nc("To add a custom value, not in the predefined list", "Custom"), value: -1 },
    ]

    /// The options for the grace period.
    property var lockGraceOptions: [
        { index: 0, text: i18nc("The grace period is disabled", "Require password immediately"), unit: "minutes", value: 0 },
        { index: 1, text: i18n("5 seconds"), unit: "seconds", value: 5 },
        { index: 2, text: i18n("30 seconds"), unit: "seconds", value: 30 },
        { index: 3, text: i18n("1 minute"), unit: "minutes", value: 1 },
        { index: 4, text: i18n("5 minutes"), unit: "minutes", value: 5 },
        { index: 5, text: i18n("15 minutes"), unit: "minutes", value: 15 },
        { index: 6, text: i18nc("To add a custom value, not in the predefined list", "Custom"), value: -1 },
    ]

    ColumnLayout {
        spacing: 0

        Kirigami.FormLayout {
            QQC2.ComboBox {
                id: timeoutComboBox
                Kirigami.FormData.label: i18n("Lock screen automatically:")
                textRole: "text"
                model: root.timeoutOptions
                currentIndex: timeoutOptionsCurrentIndexForSavedValue()

                onCurrentIndexChanged: {
                    if (currentIndex === -1) {
                        return;
                    }

                    if (model[currentIndex].value === -1) {
                        customTimeoutPromptDialogLoader.load();

                        // Pass the current value to the dialog so it
                        // can be pre-filled in the input field.
                        customTimeoutPromptDialogLoader.item.value = kcm.settings.timeout;
                        customTimeoutPromptDialogLoader.item.open();
                    } else {
                        kcm.settings.timeout = model[currentIndex].value;
                        const enableAutolock = (currentIndex !== 0);
                        kcm.settings.autolock = enableAutolock;
                    }
                }

                Component.onCompleted: {
                    const index = timeoutOptionsCurrentIndexForSavedValue();

                    // The index will be -1 if the saved value is a custom option.
                    if (index !== -1) {
                        return;
                    }

                    // If it is custom, we need to add it to the model on startup.
                    const customOptionIndex = root.timeoutOptions.length;
                    root.timeoutOptions.push({ index: customOptionIndex, text: i18np("%1 minute", "%1 minutes", kcm.settings.timeout), value: kcm.settings.timeout, isCustom: true });
                    timeoutComboBox.model = root.timeoutOptions;
                    timeoutComboBox.currentIndex = customOptionIndex;
                }

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "Timeout"
                }

                function resetIndex() {
                    timeoutComboBox.currentIndex = timeoutOptionsCurrentIndexForSavedValue();
                }
            }

            QQC2.CheckBox {
                text: i18nc("@option:check", "Lock after waking from sleep")
                checked: kcm.settings.lockOnResume
                onToggled: kcm.settings.lockOnResume = checked

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "LockOnResume"
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            QQC2.ComboBox {
                id: lockGraceComboBox
                Kirigami.FormData.label: i18nc("First part of sentence \"Delay before password required: X minutes\"", "Delay before password required:")
                textRole: "text"
                model: root.lockGraceOptions
                currentIndex: lockGraceOptionsCurrentIndexForSavedValue()

                onCurrentIndexChanged: {
                    if (currentIndex === -1) {
                        return;
                    }

                    if (model[currentIndex].value === -1) {
                        customLockGracePromptDialogLoader.load();

                        const currentOptionIndex = lockGraceOptionsCurrentIndexForSavedValue();
                        const currentOption = root.lockGraceOptions[currentOptionIndex];

                        customLockGracePromptDialogLoader.item.valueType = (currentOption.unit === "minutes")
                            ? DurationPromptDialog.ValueType.Minutes
                            : DurationPromptDialog.ValueType.Seconds;

                        customLockGracePromptDialogLoader.item.value = currentOption.value;
                        customLockGracePromptDialogLoader.item.open();
                    } else {
                        let newValueInSeconds;
                        if (model[currentIndex].unit === "minutes") {
                            newValueInSeconds = model[currentIndex].value * 60;
                        } else {
                            newValueInSeconds = model[currentIndex].value;
                        }

                        kcm.settings.lockGrace = newValueInSeconds;
                    }
                }

                Component.onCompleted: {
                    const index = lockGraceOptionsCurrentIndexForSavedValue();

                    // The index will be -1 if the saved value is a custom option.
                    if (index !== -1) {
                        return;
                    }

                    // If it is custom, we need to add it to the model on startup.
                    const customOptionIndex = root.lockGraceOptions.length;
                    let customOptionValue = kcm.settings.lockGrace;
                    const isMinutes = customOptionValue % 60 === 0;
                    
                    if (isMinutes) {
                        customOptionValue = customOptionValue / 60;
                    }

                    const customOptionUnit = isMinutes ? "minutes" : "seconds";
                    const text = isMinutes
                        ? i18np("%1 minute", "%1 minutes", customOptionValue)
                        : i18np("%1 second", "%1 seconds", customOptionValue);
                    root.lockGraceOptions.push({ index: customOptionIndex, text: text, unit: customOptionUnit, value: customOptionValue, isCustom: true });
                    lockGraceComboBox.model = root.lockGraceOptions;
                    lockGraceComboBox.currentIndex = customOptionIndex;
                }

                KCM.SettingStateBinding {
                    extraEnabledConditions: kcm.settings.autolock
                    configObject: kcm.settings
                    settingName: "LockGrace"
                }

                function resetIndex() {
                    lockGraceComboBox.currentIndex = lockGraceOptionsCurrentIndexForSavedValue();
                }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
            }

            KQuickControls.KeySequenceItem {
                Kirigami.FormData.label: i18n("Keyboard shortcut:")
                keySequence: kcm.settings.shortcut
                onCaptureFinished: kcm.settings.shortcut = keySequence

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "shortcut"
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            QQC2.Button {
                Kirigami.FormData.label: i18n("Appearance:")
                text: i18nc("@action:button", "Configure...")
                icon.name: "preferences-desktop-theme"
                onClicked: kcm.push("Appearance.qml")

                KCM.SettingHighlighter {
                    highlight: !kcm.isDefaultsAppearance
                }
            }
        }
    }

    /// Return the index of the timeout option that matches the saved value.
    function timeoutOptionsCurrentIndexForSavedValue() {
        if (!kcm.settings.autolock) {
            return 0;
        }

        let targetOption;
        const savedValue = kcm.settings.timeout;

        // Check for an existing option that matches the saved value.
        targetOption = root.timeoutOptions.find(function (item) { return item.value === savedValue });
        if (targetOption !== undefined) {
            return targetOption.index;
        }

        return -1;
    }

    /// Return the index of the lockGrace option that matches the saved value.
    function lockGraceOptionsCurrentIndexForSavedValue() {
        let targetOption;
        const savedValueInSeconds = kcm.settings.lockGrace;
        const isMinutes = savedValueInSeconds % 60 === 0;

        // Check for an existing option that matches the saved value in seconds.
        targetOption = root.lockGraceOptions.find(function (item) { return item.unit === "seconds" && item.value === savedValueInSeconds });
        if (targetOption !== undefined) {
            return targetOption.index;
        }

        // Check for an existing option that matches the saved value in minutes.
        targetOption = root.lockGraceOptions.find(function (item) { return item.unit === "minutes" && item.value === savedValueInSeconds / 60 });
        if (targetOption !== undefined) {
            return targetOption.index;
        }

        return -1;
    }

    /// Dialog handled by a Loader to avoid loading it until it is needed.
    Loader {
        id: customTimeoutPromptDialogLoader
        anchors.centerIn: parent

        /// Load the dialog if it is not already loaded.
        function load() {
            if (status === Loader.Null) {
                sourceComponent = customTimeoutPromptDialogComponent;
            }
        }
    }

    /// Component prevents the dialog from being loaded until the loader loads it.
    Component {
        id: customTimeoutPromptDialogComponent

        DurationPromptDialog {
            id: customTimeoutPromptDialog
            title: i18nc("@title:window", "Custom Duration")
            subtitle: timeoutComboBox.Kirigami.FormData.label

            acceptsMinutes: true
            acceptsSeconds: false
            valueType: DurationPromptDialog.ValueType.Minutes

            onAccepted: function() {
                setCustomTimeout(customTimeoutPromptDialog.value);
                customTimeoutPromptDialog.close();
            }

            onRejected: function() {
                customTimeoutPromptDialog.close();
                // Reset the target to the previous value, otherwise the selected
                // option will still say "Custom" after the dialog is closed.
                timeoutComboBox.currentIndex = timeoutOptionsCurrentIndexForSavedValue();
            }

            /// Set the custom timeout the user entered in the dialog.
            function setCustomTimeout(customTime) {
                // If the requested time is already in the model, just select it.
                const timeoutOption = timeoutComboBox.model.find(function (item) { return item.value === customTime });
                if (timeoutOption !== undefined) {
                    timeoutComboBox.currentIndex = timeoutOption.index;
                    return;
                }

                // Ensure the model has the default options.
                const options = root.timeoutOptions.filter(function (item) { return !item.isCustom });
                
                // Add the custom option.
                const customIndex = options.length;
                options.push({ index: customIndex, text: i18np("%1 minute", "%1 minutes", customTime), value: customTime, isCustom: true });
                root.timeoutOptions = options;
                timeoutComboBox.model = root.timeoutOptions;
                timeoutComboBox.currentIndex = customIndex;
            }
        }
    }

    /// Dialog handled by a Loader to avoid loading it until it is needed.
    Loader {
        id: customLockGracePromptDialogLoader
        anchors.centerIn: parent

        /// Load the dialog if it is not already loaded.
        function load() {
            if (status === Loader.Null) {
                sourceComponent = customLockGracePromptDialogComponent;
            }
        }
    }

    /// Component prevents the dialog from being loaded until the loader loads it.
    Component {
        id: customLockGracePromptDialogComponent

        DurationPromptDialog {
            id: customLockGracePromptDialog
            title: i18nc("@title:window", "Custom Duration")
            subtitle: lockGraceComboBox.Kirigami.FormData.label

            acceptsMinutes: true
            acceptsSeconds: true

            onAccepted: function() {
                const isMinutes = customLockGracePromptDialog.valueType === DurationPromptDialog.ValueType.Minutes;
                setCustomLockGrace(customLockGracePromptDialog.value, isMinutes);
                customLockGracePromptDialog.close();
            }

            onRejected: function() {
                customLockGracePromptDialog.close();
                // Reset the target to the previous value, otherwise the selected
                // option will still say "Custom" after the dialog is closed.
                lockGraceComboBox.currentIndex = lockGraceOptionsCurrentIndexForSavedValue();
            }

            /// Set the custom grace period the user entered in the dialog.
            function setCustomLockGrace(customTime, isMinutes) {
                const customTimeInSeconds = customTime * (isMinutes ? 60 : 1);

                // If the requested time is already in the model, just select it.
                const lockGraceOption = lockGraceComboBox.model.find(function (item) {
                    // If isMinutes is true, the item should have a unit of "minutes".
                    if (isMinutes) {
                        return item.value === customTime && item.unit === "minutes";
                    } else {
                        return item.value === customTimeInSeconds && item.unit === "seconds";
                    }
                });

                if (lockGraceOption !== undefined) {
                    lockGraceComboBox.currentIndex = lockGraceOption.index;
                    return;
                }

                // Ensure the model has the default options.
                const options = root.lockGraceOptions.filter(function (item) { return !item.isCustom });

                // Add the custom option.
                const customIndex = options.length;
                
                if (isMinutes) {
                    options.push({ index: customIndex, text: i18np("%1 minute", "%1 minutes", customTime), unit: "minutes", value: customTime, isCustom: true });
                } else {
                    options.push({ index: customIndex, text: i18np("%1 second", "%1 seconds", customTimeInSeconds), unit: "seconds", value: customTimeInSeconds, isCustom: true });
                }

                root.lockGraceOptions = options;
                lockGraceComboBox.model = root.lockGraceOptions;
                lockGraceComboBox.currentIndex = customIndex;
            }
        }
    }

    /// Connection to ensure the combo boxes are reset when the KCM's `Defaults` 
    /// or `Reset` buttons are clicked.
    Connections {
        target: kcm

        /// The KCM `Defaults` button was clicked.
        function onDefaultsCalled() {
            resetComboBoxes();
        }

        /// The KCM `Reset` button was clicked.
        function onLoadCalled() {
            resetComboBoxes();
        }

        /// Set the combo boxes to the saved values.
        function resetComboBoxes() {
            timeoutComboBox.resetIndex();
            lockGraceComboBox.resetIndex();
        }
    }
}
