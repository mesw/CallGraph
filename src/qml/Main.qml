import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    title: "CallGraph"
    width: 1400
    height: 900
    visible: true

    // Injected from main.cpp via setContextProperty
    property var controller: null

    // Export dialog state
    property string pendingExportFormat: "mermaid"

    // ---------------------------------------------------------------------------
    // Toolbar
    // ---------------------------------------------------------------------------
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            // Source root field
            TextField {
                id: sourceRootField
                Layout.preferredWidth: 340
                text: controller ? controller.settings.sourceRoot : ""
                placeholderText: "Source root (git working tree)…"
                font.pixelSize: 12
                onEditingFinished: {
                    if (controller) controller.settings.sourceRoot = text
                }
            }

            Button {
                text: "Browse…"
                font.pixelSize: 12
                onClicked: folderDialog.open()
            }

            Button {
                text: controller && controller.indexBuilding ? "Building…" : "Build Index"
                enabled: controller && !controller.indexBuilding && sourceRootField.text.length > 0
                font.pixelSize: 12
                onClicked: {
                    controller.settings.sourceRoot = sourceRootField.text
                    controller.settings.save()
                    controller.buildIndex()
                }
            }

            // Status label
            Label {
                text: controller ? controller.indexStatus : ""
                font.pixelSize: 11
                color: "#555"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Filter toggles
            Label { text: "Edges:"; font.pixelSize: 11; color: "#555" }
            ComboBox {
                id: edgeFilterCombo
                model: ["All", "Direct only", "No Unresolved"]
                font.pixelSize: 11
                implicitWidth: 120
                onCurrentIndexChanged: {
                    if (!controller) return
                    switch (currentIndex) {
                    case 0: controller.edgeFilter = 0xFF; break
                    case 1: controller.edgeFilter = 0x03; break  // DirectCall|MethodCall
                    case 2: controller.edgeFilter = 0xFF & ~0x40; break  // all except Unresolved
                    }
                }
            }

            // Depth selector
            Label { text: "Export depth:"; font.pixelSize: 11; color: "#555" }
            SpinBox {
                from: 1; to: 10; value: controller ? controller.settings.exportDepth : 3
                font.pixelSize: 11
                implicitWidth: 70
                onValueChanged: if (controller) controller.settings.exportDepth = value
            }

            // Export button
            Button {
                text: "Export…"
                font.pixelSize: 12
                enabled: controller && controller.indexReady && controller.focusId !== 0
                onClicked: exportMenu.open()

                Menu {
                    id: exportMenu
                    MenuItem {
                        text: "Mermaid (.md)"
                        onTriggered: { pendingExportFormat = "mermaid"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "Graphviz DOT (.dot)"
                        onTriggered: { pendingExportFormat = "dot"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "draw.io XML (.xml)"
                        onTriggered: { pendingExportFormat = "drawio"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "JSON (.json)"
                        onTriggered: { pendingExportFormat = "json"; saveDialog.open() }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Main layout: Search | Focus | Details
    // ---------------------------------------------------------------------------
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        SearchPane {
            id: searchPane
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 180
            controller: window.controller
        }

        FocusPane {
            id: focusPane
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 200
            controller: window.controller
        }

        DetailsPane {
            id: detailsPane
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            controller: window.controller
        }
    }

    // ---------------------------------------------------------------------------
    // Status bar
    // ---------------------------------------------------------------------------
    footer: Rectangle {
        height: 24
        color: "#f0f0f0"
        border.color: "#ddd"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 16

            Label {
                text: controller
                      ? controller.symbolCount + " symbols, " + controller.edgeCount + " edges"
                      : "No index"
                font.pixelSize: 11
                color: "#555"
            }

            Label {
                visible: controller && controller.parseErrors.length > 0
                text: controller ? controller.parseErrors.length + " parse error(s)" : ""
                font.pixelSize: 11
                color: "#c00"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: errorDialog.open()
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: controller && controller.focusQName.length > 0
                text: controller ? "Focus: " + controller.focusQName : ""
                font.pixelSize: 11
                color: "#555"
                elide: Text.ElideLeft
                Layout.preferredWidth: 300
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Dialogs
    // ---------------------------------------------------------------------------
    FolderDialog {
        id: folderDialog
        title: "Select source root"
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "").replace("file://", "")
            sourceRootField.text = path
            if (controller) controller.settings.sourceRoot = path
        }
    }

    FileDialog {
        id: saveDialog
        title: "Export to…"
        fileMode: FileDialog.SaveFile
        nameFilters: {
            switch (pendingExportFormat) {
            case "mermaid": return ["Mermaid files (*.md *.mmd)", "All files (*)"]
            case "dot":     return ["DOT files (*.dot *.gv)", "All files (*)"]
            case "drawio":  return ["draw.io XML (*.xml)", "All files (*)"]
            case "json":    return ["JSON files (*.json)", "All files (*)"]
            default:        return ["All files (*)"]
            }
        }
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "").replace("file://", "")
            if (controller) controller.exportSlice(pendingExportFormat, path)
        }
    }

    Dialog {
        id: errorDialog
        title: "Parse Errors"
        width: 600
        height: 400
        standardButtons: Dialog.Close

        ScrollView {
            anchors.fill: parent
            TextArea {
                readOnly: true
                text: controller ? controller.parseErrors.join("\n") : ""
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }
        }
    }

    // Export success/failure notifications
    Connections {
        target: controller

        function onExportDone(filePath) {
            exportNotif.text = "Exported to: " + filePath
            exportNotif.color = "#2e7d32"
            exportNotif.visible = true
            exportNotifTimer.restart()
        }

        function onExportError(message) {
            exportNotif.text = "Export error: " + message
            exportNotif.color = "#c62828"
            exportNotif.visible = true
            exportNotifTimer.restart()
        }
    }

    Rectangle {
        id: exportNotif
        visible: false
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 40
        radius: 6
        color: "#2e7d32"
        implicitWidth: notifLabel.implicitWidth + 24
        implicitHeight: notifLabel.implicitHeight + 12
        z: 10

        property alias text: notifLabel.text

        Label {
            id: notifLabel
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 12
        }

        Timer {
            id: exportNotifTimer
            interval: 3000
            onTriggered: exportNotif.visible = false
        }
    }
}
