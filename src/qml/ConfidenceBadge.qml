import QtQuick 2.15

Rectangle {
    id: root
    property int confidence: 0
    property string confidenceLabel: ""

    implicitWidth: label.implicitWidth + 8
    implicitHeight: label.implicitHeight + 4
    radius: 3

    color: {
        switch (confidence) {
        case 0: return "#e8f5e9"  // Exact – green tint
        case 1: return "#fff8e1"  // Overload – amber tint
        case 2: return "#fce4ec"  // Heuristic – pink tint
        case 3: return "#f5f5f5"  // Unknown
        default: return "#f5f5f5"
        }
    }
    border.color: Qt.darker(color, 1.3)
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        text: root.confidenceLabel
        font.pixelSize: 10
        color: "#333333"
    }
}
