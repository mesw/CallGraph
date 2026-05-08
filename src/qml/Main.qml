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

    // 'controller' is injected via engine.rootContext()->setContextProperty()
    // and is accessible as a plain identifier in all QML files in this engine.
    // Do NOT redeclare it here — that would shadow the context property with null.

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

            TextField {
                id: sourceRootField
                Layout.preferredWidth: 340
                text: controller.settings.sourceRoot
                placeholderText: "Source root (git working tree)…"
                font.pixelSize: 12
                onEditingFinished: controller.settings.sourceRoot = text
            }

            Button {
                text: "Browse…"
                font.pixelSize: 12
                onClicked: folderDialog.open()
            }

            Button {
                text: controller.indexBuilding ? "Building…" : "Build Index"
                enabled: !controller.indexBuilding && sourceRootField.text.length > 0
                font.pixelSize: 12
                onClicked: {
                    controller.settings.sourceRoot = sourceRootField.text
                    controller.settings.save()
                    controller.buildIndex()
                }
            }

            Label {
                text: controller.indexStatus
                font.pixelSize: 11
                color: "#555"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label { text: "Edges:"; font.pixelSize: 11; color: "#555" }
            ComboBox {
                id: edgeFilterCombo
                model: ["All", "Direct only", "No Unresolved"]
                font.pixelSize: 11
                implicitWidth: 120
                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: controller.edgeFilter = 0xFF; break
                    case 1: controller.edgeFilter = 0x03; break
                    case 2: controller.edgeFilter = 0xBF; break  // 0xFF & ~0x40 (Unresolved bit)
                    }
                }
            }

            Label { text: "Export depth:"; font.pixelSize: 11; color: "#555" }
            SpinBox {
                from: 1; to: 10; value: controller.settings.exportDepth
                font.pixelSize: 11
                implicitWidth: 70
                onValueChanged: controller.settings.exportDepth = value
            }

            Button {
                text: "Export…"
                font.pixelSize: 12
                enabled: controller.indexReady && controller.focusId !== 0
                onClicked: exportMenu.open()

                Menu {
                    id: exportMenu
                    MenuItem {
                        text: "Mermaid (.md)"
                        onTriggered: { window.pendingExportFormat = "mermaid"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "Graphviz DOT (.dot)"
                        onTriggered: { window.pendingExportFormat = "dot"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "draw.io XML (.xml)"
                        onTriggered: { window.pendingExportFormat = "drawio"; saveDialog.open() }
                    }
                    MenuItem {
                        text: "JSON (.json)"
                        onTriggered: { window.pendingExportFormat = "json"; saveDialog.open() }
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
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 180
        }

        FocusPane {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 200
        }

        DetailsPane {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
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
                text: controller.symbolCount + " symbols, " + controller.edgeCount + " edges"
                font.pixelSize: 11
                color: "#555"
            }

            Label {
                visible: controller.parseErrors.length > 0
                text: controller.parseErrors.length + " parse error(s)"
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
                visible: controller.focusQName.length > 0
                text: "Focus: " + controller.focusQName
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
            var path = selectedFolder.toString().replace(/^file:\/\/\//, "").replace(/^file:\/\//, "")
            sourceRootField.text = path
            controller.settings.sourceRoot = path
        }
    }

    FileDialog {
        id: saveDialog
        title: "Export to…"
        fileMode: FileDialog.SaveFile
        nameFilters: {
            switch (window.pendingExportFormat) {
            case "mermaid": return ["Mermaid files (*.md *.mmd)", "All files (*)"]
            case "dot":     return ["DOT files (*.dot *.gv)", "All files (*)"]
            case "drawio":  return ["draw.io XML (*.xml)", "All files (*)"]
            case "json":    return ["JSON files (*.json)", "All files (*)"]
            default:        return ["All files (*)"]
            }
        }
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\/\//, "").replace(/^file:\/\//, "")
            controller.exportSlice(window.pendingExportFormat, path)
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
                text: controller.parseErrors.join("\n")
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }
        }
    }

    Connections {
        target: controller
        function onExportDone(filePath) {
            exportNotif.notifColor = "#2e7d32"
            exportNotif.text = "Exported to: " + filePath
            exportNotif.visible = true
            exportNotifTimer.restart()
        }
        function onExportError(message) {
            exportNotif.notifColor = "#c62828"
            exportNotif.text = "Export error: " + message
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
        z: 10
        property string notifColor: "#2e7d32"
        property alias text: notifLabel.text
        color: notifColor
        implicitWidth: notifLabel.implicitWidth + 24
        implicitHeight: notifLabel.implicitHeight + 12
        Label { id: notifLabel; anchors.centerIn: parent; color: "white"; font.pixelSize: 12 }
        Timer { id: exportNotifTimer; interval: 3000; onTriggered: exportNotif.visible = false }
    }
}
