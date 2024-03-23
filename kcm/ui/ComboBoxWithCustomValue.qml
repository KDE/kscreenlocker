/*
    SPDX-FileCopyrightText: 2024 Kristen McWilliam <kmcwilliampublic@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/**
 * A combobox that displays a list of options.
 */
QQC2.ComboBox {
    id: root

    function modelWithCustomOptionTrigger(model) {
        if (model.length === 0) {
            return model;
        }

        // If the last item in the model is the custom value trigger, return.
        if (model[model.length - 1].setCustomTigger) {
            return model;
        }

        // If the model contains a custom value trigger that is not the last 
        // item, remove it.
        for (var i = 0; i < model.length; i++) {
            if (model[i].setCustomTigger) {
                model.splice(i, 1);
                break;
            }
        }
        
        // Add an item the user can select to set a custom value.
        model.push({ index: model.length, text: i18nc("To add a custom value, not in the predefined list", "Custom"), value: -1, setCustomTigger: true });
        return model;
    }

    Component.onCompleted: {
        console.info("completed");
        model = Qt.binding(() => modelWithCustomOptionTrigger(model));
        console.info("model: " + model);
    }

    onCurrentIndexChanged: {
        console.info("current index changed");
        
        if (model[currentIndex].setCustomTigger) {
            console.info("custom trigger selected");
            customPromptDialogLoader.load();
            customPromptDialogLoader.item.value = 0;
            customPromptDialogLoader.item.open();
        }
    }

    //
    // Check the indexOfValue() method.
    // https://doc.qt.io/qt-5/qml-qtquick-controls2-combobox.html#indexOfValue-method
    // Might be helpful here?
    //

    /// Dialog handled by a Loader to avoid loading it until it is needed.
    Loader {
        id: customPromptDialogLoader
        anchors.centerIn: parent

        /// Load the dialog if it is not already loaded.
        function load() {
            if (status === Loader.Null) {
                sourceComponent = customPromptDialogComponent;
            }
        }
    }

    /// Component prevents the dialog from being loaded until the loader loads it.
    Component {
        id: customPromptDialogComponent

        DurationPromptDialog {
            id: customPromptDialog
            title: i18nc("@title:window", "Custom Duration")
            subtitle: root.Kirigami.FormData.label

            acceptsMinutes: true
            acceptsSeconds: false
            valueType: DurationPromptDialog.ValueType.Minutes

            onAccepted: function() {
                // setCustomTimeout(customPromptDialog.value);
                customPromptDialog.close();
            }

            onRejected: function() {
                customPromptDialog.close();
                // Reset the target to the previous value, otherwise the selected
                // option will still say "Custom" after the dialog is closed.
                // timeoutComboBox.currentIndex = timeoutOptionsCurrentIndexForSavedValue();
            }

            /// Set the custom timeout the user entered in the dialog.
            function setCustomTimeout(customTime) {
                // // If the requested time is already in the model, just select it.
                // const timeoutOption = timeoutComboBox.model.find(function (item) { return item.value === customTime });
                // if (timeoutOption !== undefined) {
                //     timeoutComboBox.currentIndex = timeoutOption.index;
                //     return;
                // }

                // // Ensure the model has the default options.
                // const options = root.timeoutOptions.filter(function (item) { return !item.isCustom });
                
                // // Add the custom option.
                // const customIndex = options.length;
                // options.push({ index: customIndex, text: i18np("%1 minute", "%1 minutes", customTime), value: customTime, isCustom: true });
                // root.timeoutOptions = options;
                // timeoutComboBox.model = root.timeoutOptions;
                // timeoutComboBox.currentIndex = customIndex;
            }
        }
    }
}
