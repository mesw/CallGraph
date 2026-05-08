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
        spacing: 6

        // Search bar + mode toggle
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search functions…"
                onTextChanged: controller.searchQuery = text
                font.pixelSize: 13
                background: Rectangle {
                    radius: 3
                    color: "white"
                    border.color: searchField.activeFocus ? "#5b9bd5" : "#ccc"
                    border.width: 1
                }
            }

            ComboBox {
                id: modeCombo
                model: ["Substring", "Regex", "Exact"]
                currentIndex: controller.searchMode
                onCurrentIndexChanged: controller.searchMode = currentIndex
                implicitWidth: 100
                font.pixelSize: 12
            }
        }

        // Result count label
        Label {
            text: controller.searchModel.count === 0
                  ? (searchField.text.length > 0 ? "No results" : "")
                  : controller.searchModel.count + " result(s)"
            font.pixelSize: 11
            color: "#666"
        }

        // Results list
        ListView {
            id: resultsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: controller.searchModel

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                width: resultsList.width
                height: 52
                highlighted: controller.focusId === model.symbolId

                background: Rectangle {
                    color: parent.highlighted ? "#d0e8ff" : (parent.hovered ? "#edf5ff" : "transparent")
                }

                contentItem: ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: model.qualifiedName
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "(" + model.arity + ")"
                            font.pixelSize: 11
                            color: "#666"
                        }
                    }

                    Label {
                        text: {
                            var f = model.file
                            var idx = Math.max(f.lastIndexOf("/"), f.lastIndexOf("\\"))
                            return (idx >= 0 ? f.substring(idx + 1) : f) + ":" + model.line
                        }
                        font.pixelSize: 10
                        color: "#888"
                        elide: Text.ElideLeft
                        Layout.fillWidth: true
                    }
                }

                onClicked: controller.focusSymbol(model.symbolId)
            }
        }
    }
}
