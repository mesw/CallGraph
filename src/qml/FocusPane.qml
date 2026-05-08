import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#fafafa"
    border.color: "#ddd"
    border.width: 1

    // 'controller' is a QML context property — accessible without redeclaration.

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: "Focus"
            font.pixelSize: 11
            font.bold: true
            color: "#555"
        }

        // Qualified name
        Label {
            id: qnameLabel
            text: controller.focusQName || "(none selected)"
            font.pixelSize: 14
            font.bold: true
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            color: controller.focusQName ? "#1a1a1a" : "#aaa"
        }

        // Class membership
        Label {
            visible: controller.focusClass.length > 0
            text: "Class: " + controller.focusClass
            font.pixelSize: 12
            color: "#555"
        }

        // Virtual badge
        Rectangle {
            visible: controller.focusIsVirtual
            color: "#cce0ff"
            radius: 3
            implicitWidth: vLabel.implicitWidth + 8
            implicitHeight: vLabel.implicitHeight + 4
            Label { id: vLabel; text: "virtual"; anchors.centerIn: parent; font.pixelSize: 11 }
        }

        // File + line
        RowLayout {
            visible: controller.focusFile.length > 0
            Layout.fillWidth: true
            spacing: 6

            Label {
                id: fileLabel
                text: {
                    var f = controller.focusFile
                    var idx = Math.max(f.lastIndexOf("/"), f.lastIndexOf("\\"))
                    return (idx >= 0 ? f.substring(idx + 1) : f) + ":" + controller.focusLine
                }
                font.pixelSize: 12
                color: "#444"
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }

            Button {
                text: "Open"
                font.pixelSize: 11
                implicitHeight: 24
                onClicked: controller.openInEditor(controller.focusId)
            }
        }

        // Navigation buttons
        RowLayout {
            spacing: 6
            Button {
                text: "← Back"
                font.pixelSize: 11
                implicitHeight: 28
                enabled: controller.canNavigateBack()
                onClicked: controller.navigateBack()
            }
            Button {
                text: "Forward →"
                font.pixelSize: 11
                implicitHeight: 28
                enabled: controller.canNavigateForward()
                onClicked: controller.navigateForward()
            }
        }

        Item { Layout.fillHeight: true }
    }
}
