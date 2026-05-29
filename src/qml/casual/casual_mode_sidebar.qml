// casual_mode_sidebar.qml  —  l-reader · Modo Casual
//
// Sidebar com três abas funcionais:
//   0 – Sumário     → tocListModel      (CasualTocModel C++)
//   1 – Anotações   → annotListModel    (CasualAnnotModel C++)
//   2 – Marcadores  → bookmarkListModel (CasualBookmarkModel C++)
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    readonly property int openWidth: 270
    width: openWidth

    Rectangle {
        id: panel
        width:  root.openWidth
        height: root.height
        color:  casualCtrl.headerBg
        layer.enabled: true

        // Sombra lateral direita
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: casualCtrl.borderColor
            Behavior on color { ColorAnimation { duration: 220 } }
        }

        x: casualCtrl.sidebarOpen ? 0 : -root.openWidth
        Behavior on x     { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation  { duration: 220 } }

        ColumnLayout {
            anchors {
                fill:         parent
                topMargin:    8
                bottomMargin: 8
                leftMargin:   0
                rightMargin:  0
            }
            spacing: 0

            // ── Cabeçalho ──────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth:    true
                Layout.leftMargin:   16
                Layout.rightMargin:  12
                Layout.bottomMargin: 6
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text:  casualCtrl.hasDocument ? casualCtrl.bookTitle : "Nenhum documento aberto"
                        color: casualCtrl.textColor
                        font { pixelSize: 13; weight: Font.SemiBold; family: "Georgia, serif" }
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                    Text {
                        text:  casualCtrl.author.length > 0 ? casualCtrl.author : " "
                        color: casualCtrl.mutedColor
                        font { pixelSize: 11; family: "Georgia, serif" }
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }

                // Botão fechar
                Rectangle {
                    width: 28; height: 28; radius: 6
                    color: closeArea.containsMouse
                               ? Qt.alpha(casualCtrl.textColor, 0.08) : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text { anchors.centerIn: parent; text: "×"; color: casualCtrl.mutedColor; font.pixelSize: 16 }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        onClicked:    casualCtrl.setSidebarOpen(false)
                    }
                }
            }

            // Divisor
            Rectangle { Layout.fillWidth: true; height: 1; color: casualCtrl.borderColor; opacity: 0.7 }

            // ── TabBar ─────────────────────────────────────────────────────
            TabBar {
                id: tabBar
                Layout.fillWidth:   true
                Layout.topMargin:   4
                Layout.leftMargin:  4
                Layout.rightMargin: 4
                currentIndex: 0
                background: Rectangle { color: "transparent" }

                Repeater {
                    model: ["Sumário", "Anotações", "Marcadores"]
                    TabButton {
                        required property string modelData
                        required property int    index
                        text: modelData
                        width: implicitWidth
                        contentItem: Text {
                            text:  parent.text
                            color: tabBar.currentIndex === parent.index
                                       ? casualCtrl.accentColor : casualCtrl.mutedColor
                            font.pixelSize: 11
                            font.weight:    tabBar.currentIndex === parent.index
                                               ? Font.SemiBold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                        background: Rectangle {
                            Rectangle {
                                visible: tabBar.currentIndex === parent.index
                                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                height: 2; radius: 1; color: casualCtrl.accentColor
                            }
                            color: "transparent"
                        }
                    }
                }
            }

            // ── StackLayout ────────────────────────────────────────────────
            StackLayout {
                Layout.fillWidth:  true
                Layout.fillHeight: true
                Layout.topMargin:  4
                currentIndex: tabBar.currentIndex

                // ── ABA 0: SUMÁRIO ─────────────────────────────────────────
                ScrollView {
                    clip: true

                    ListView {
                        id: tocList
                        model: typeof tocListModel !== "undefined" ? tocListModel : null
                        spacing: 0

                        delegate: TocEntry {
                            required property string title
                            required property int    pageOrIndex
                            required property int    depth
                            required property bool   isCurrent

                            label:       title
                            pageNum:     pageOrIndex
                            indentLevel: depth
                            active:      isCurrent
                            width:       tocList.width
                        }
                    }

                    // Sem TOC
                    Item {
                        anchors.fill: parent
                        visible: (tocList.count === 0) && casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Este documento\nnão tem índice"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.6
                        }
                    }
                    Item {
                        anchors.fill: parent
                        visible: !casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Abra um documento\npara ver o índice"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.5
                        }
                    }
                }

                // ── ABA 1: ANOTAÇÕES ───────────────────────────────────────
                ScrollView {
                    clip: true

                    ListView {
                        id: annotList
                        model: typeof annotListModel !== "undefined" ? annotListModel : null
                        spacing: 4

                        delegate: AnnotEntry {
                            required property string annotId
                            required property string annotText
                            required property string annotColor
                            required property int    annotPage
                            required property string annotPageLabel
                            required property string annotSnippet

                            entryId:    annotId
                            snippet:    annotSnippet
                            pageLabel:  annotPageLabel
                            hlColor:    annotColor
                            rowIndex:   index
                            width:      annotList.width
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: (annotList.count === 0) && casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Sem anotações\nneste documento"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.6
                        }
                    }
                    Item {
                        anchors.fill: parent
                        visible: !casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Abra um documento\npara ver as anotações"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.5
                        }
                    }
                }

                // ── ABA 2: MARCADORES ──────────────────────────────────────
                ScrollView {
                    clip: true

                    ListView {
                        id: bookmarkList
                        model: typeof bookmarkListModel !== "undefined" ? bookmarkListModel : null
                        spacing: 0

                        delegate: BookmarkEntry {
                            required property int    bmPage
                            required property string bmLabel
                            required property string bmPageLabel

                            page:      bmPage
                            label:     bmLabel
                            pageLabel: bmPageLabel
                            rowIndex:  index
                            width:     bookmarkList.width
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: (bookmarkList.count === 0) && casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Sem marcadores\nneste documento"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.6
                        }
                    }
                    Item {
                        anchors.fill: parent
                        visible: !casualCtrl.hasDocument
                        Text {
                            anchors.centerIn: parent
                            text: "Abra um documento\npara ver os marcadores"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.5
                        }
                    }
                }
            }
        }
    }

    // ── Componentes inline ─────────────────────────────────────────────────

    // TocEntry — entrada do sumário
    component TocEntry: Item {
        property string label:       ""
        property int    pageNum:     0
        property int    indentLevel: 0
        property bool   active:      false

        height: 34

        Rectangle {
            anchors.fill: parent
            color: active
                       ? Qt.alpha(casualCtrl.accentColor, 0.10)
                       : (entryArea.containsMouse
                              ? Qt.alpha(casualCtrl.textColor, 0.05)
                              : "transparent")
            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors {
                    fill:        parent
                    leftMargin:  12 + indentLevel * 14
                    rightMargin: 12
                }
                spacing: 4

                Rectangle {
                    visible: active
                    width: 3; height: 16; radius: 1.5
                    color: casualCtrl.accentColor
                }

                Text {
                    Layout.fillWidth: true
                    text:  label
                    elide: Text.ElideRight
                    color: active ? casualCtrl.accentColor : casualCtrl.textColor
                    font.pixelSize: indentLevel > 0 ? 11 : 12
                    font.weight:    active ? Font.Medium : Font.Normal
                    font.family:    "Georgia, serif"
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                Text {
                    text:  pageNum > 0 ? String(pageNum + 1) : ""
                    color: casualCtrl.mutedColor
                    font { pixelSize: 10; family: "Georgia, serif" }
                }
            }

            MouseArea {
                id: entryArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked: {
                    if (casualCtrl.chapterCount > 0)
                        casualCtrl.setCurrentChapterIndex(pageOrIndex)
                    else
                        casualCtrl.navigateToPage(pageOrIndex)
                    casualCtrl.setSidebarOpen(false)
                }
            }
        }
    }

    // AnnotEntry — highlight/anotação
    component AnnotEntry: Item {
        property string entryId:   ""
        property string snippet:   ""
        property string pageLabel: ""
        property string hlColor:   "#FFFFAA"
        property int    rowIndex:  0

        height: annotContent.implicitHeight + 16

        Rectangle {
            anchors.fill: parent
            color: annotArea.containsMouse
                       ? Qt.alpha(casualCtrl.textColor, 0.04) : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }

            RowLayout {
                id: annotContent
                anchors {
                    fill:         parent
                    leftMargin:   12
                    rightMargin:  8
                    topMargin:    8
                    bottomMargin: 8
                }
                spacing: 8

                // Faixa de cor do highlight
                Rectangle {
                    width:  4
                    Layout.fillHeight: true
                    radius: 2
                    color:  hlColor
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text:  snippet.length > 0 ? snippet : "(sem texto)"
                        color: casualCtrl.textColor
                        font { pixelSize: 11; family: "Georgia, serif" }
                        wrapMode: Text.WordWrap
                        opacity:  0.9
                    }
                    Text {
                        text:  pageLabel
                        color: casualCtrl.mutedColor
                        font.pixelSize: 10
                    }
                }

                // Botão remover
                Rectangle {
                    width: 20; height: 20; radius: 4
                    color: rmAnnotArea.containsMouse
                               ? Qt.alpha(casualCtrl.textColor, 0.10) : "transparent"
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: casualCtrl.mutedColor
                        font.pixelSize: 13
                        opacity: rmAnnotArea.containsMouse ? 1.0 : 0.5
                    }
                    MouseArea {
                        id: rmAnnotArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        onClicked: {
                            if (typeof annotListModel !== "undefined")
                                annotListModel.requestRemove(rowIndex)
                        }
                    }
                }
            }

            MouseArea {
                id: annotArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked: {
                    if (typeof annotListModel !== "undefined") {
                        annotListModel.requestNavigate(rowIndex)
                    }
                    casualCtrl.setSidebarOpen(false)
                }
            }
        }
    }

    // BookmarkEntry — marcador
    component BookmarkEntry: Item {
        property int    page:      0
        property string label:     ""
        property string pageLabel: ""
        property int    rowIndex:  0

        height: 38

        Rectangle {
            anchors.fill: parent
            color: bmArea.containsMouse
                       ? Qt.alpha(casualCtrl.textColor, 0.05) : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }

            RowLayout {
                anchors {
                    fill:        parent
                    leftMargin:  14
                    rightMargin: 8
                }
                spacing: 8

                // Ícone bookmark
                Text {
                    text:  "🔖"
                    font.pixelSize: 12
                    opacity: 0.7
                }

                Text {
                    Layout.fillWidth: true
                    text:  label
                    elide: Text.ElideRight
                    color: casualCtrl.textColor
                    font { pixelSize: 12; family: "Georgia, serif" }
                }

                Text {
                    text:  pageLabel
                    color: casualCtrl.mutedColor
                    font.pixelSize: 10
                }

                // Botão remover
                Rectangle {
                    width: 20; height: 20; radius: 4
                    color: rmBmArea.containsMouse
                               ? Qt.alpha(casualCtrl.textColor, 0.10) : "transparent"
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: casualCtrl.mutedColor
                        font.pixelSize: 13
                        opacity: rmBmArea.containsMouse ? 1.0 : 0.5
                    }
                    MouseArea {
                        id: rmBmArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bookmarkListModel !== "undefined")
                                bookmarkListModel.requestRemove(rowIndex)
                        }
                    }
                }
            }

            MouseArea {
                id: bmArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked: {
                    if (typeof bookmarkListModel !== "undefined") {
                        bookmarkListModel.requestNavigate(rowIndex)
                    }
                    casualCtrl.setSidebarOpen(false)
                }
            }
        }
    }
}
