import QtQuick 2.15

Rectangle {
    id: root
    property int edgeKind: 0
    property string edgeKindLabel: ""

    implicitWidth: label.implicitWidth + 8
    implicitHeight: label.implicitHeight + 4
    radius: 3

    color: {
        switch (edgeKind) {
        case 0: return "#e0e0e0"  // DirectCall
        case 1: return "#e0e0e0"  // MethodCall
        case 2: return "#cce0ff"  // VirtualCandidate
        case 3: return "#d0f0d0"  // StoredAsPointer
        case 4: return "#ffd0d0"  // PassedToThread
        case 5: return "#e8e8e8"  // ViaMacro
        case 6: return "#f0f0f0"  // Unresolved
        default: return "#e0e0e0"
        }
    }
    border.color: Qt.darker(color, 1.2)
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        text: root.edgeKindLabel
        font.pixelSize: 10
        color: "#333333"
    }
}
