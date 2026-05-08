import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#f8f8f8"
    border.color: "#ddd"
    border.width: 1

    // 'controller' is a QML context property — accessible without redeclaration.

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // Callers
        Label {
            text: "Callers (" + controller.callersModel.count + ")"
            font.pixelSize: 12
            font.bold: true
            color: "#333"
        }

        ListView {
            id: callersList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: controller.callersModel
            ScrollBar.vertical: ScrollBar {}

            delegate: edgeDelegate
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

        // Callees
        Label {
            text: "Callees (" + controller.calleesModel.count + ")"
            font.pixelSize: 12
            font.bold: true
            color: "#333"
        }

        ListView {
            id: calleesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: controller.calleesModel
            ScrollBar.vertical: ScrollBar {}

            delegate: edgeDelegate
        }
    }

    // Shared delegate for both callers and callees lists
    Component {
        id: edgeDelegate

        ItemDelegate {
            width: ListView.view ? ListView.view.width : 0
            height: 56

            background: Rectangle {
                color: parent.hovered ? "#edf5ff" : "transparent"
            }

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 2

                // Name row
                Label {
                    text: model.neighborQName
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    color: model.isResolved ? "#1a1a1a" : "#888"
                }

                // Badges row
                RowLayout {
                    spacing: 4

                    EdgeKindBadge {
                        edgeKind: model.edgeKind
                        edgeKindLabel: model.edgeKindLabel
                    }
                    ConfidenceBadge {
                        confidence: model.confidence
                        confidenceLabel: model.confidenceLabel
                    }
                    Label {
                        visible: model.viaMacro.length > 0
                        text: "↻ " + model.viaMacro
                        font.pixelSize: 10
                        color: "#666"
                    }

                    Item { Layout.fillWidth: true }

                    // File:line
                    Label {
                        text: {
                            var f = model.neighborFile
                            if (!f) return ""
                            var idx = Math.max(f.lastIndexOf("/"), f.lastIndexOf("\\"))
                            return (idx >= 0 ? f.substring(idx + 1) : f) + ":" + model.neighborLine
                        }
                        font.pixelSize: 10
                        color: "#888"
                        elide: Text.ElideLeft
                    }
                }
            }

            onClicked: controller.focusSymbol(model.neighborId)
        }
    }
}
