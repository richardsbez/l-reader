// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/casual_mode_sidebar.qml  —  l-reader · Modo Casual
//
// Mudanças em relação à versão anterior:
//   • "Umberto Eco" hardcoded → casualCtrl.author  (dados reais)
//   • ListModel mockado da TOC → TocListModel (QAbstractListModel C++)
//   • Aba Notas e Marcadores mantidas como placeholders (dependem de
//     NoteManager e BookmarkManager — fora do escopo deste wiring)
//   • TocEntry.onClicked: setCurrentChapterIndex() para EPUB,
//                         setCurrentPage() para PDF
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    readonly property int openWidth: 260
    width: openWidth

    Rectangle {
        id: panel
        width:  root.openWidth
        height: root.height
        color:  casualCtrl.headerBg
        layer.enabled: true

        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: casualCtrl.borderColor
            Behavior on color { ColorAnimation { duration: 220 } }
        }

        x: casualCtrl.sidebarOpen ? 0 : -root.openWidth
        Behavior on x       { NumberAnimation   { duration: 280; easing.type: Easing.OutCubic } }
        Behavior on color   { ColorAnimation    { duration: 220 } }

        ColumnLayout {
            anchors {
                fill:         parent
                topMargin:    8
                bottomMargin: 8
                leftMargin:   0
                rightMargin:  0
            }
            spacing: 0

            // ── Cabeçalho ─────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth:   true
                Layout.leftMargin:  16
                Layout.rightMargin: 12
                Layout.bottomMargin: 6
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    // Título real do livro
                    Text {
                        text:  casualCtrl.hasDocument ? casualCtrl.bookTitle : "Nenhum documento aberto"
                        color: casualCtrl.textColor
                        font { pixelSize: 13; weight: Font.SemiBold; family: "Georgia, serif" }
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                    // Autor real do documento
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
                               ? Qt.alpha(casualCtrl.textColor, 0.08)
                               : "transparent"
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
            Rectangle {
                Layout.fillWidth: true
                height: 1; color: casualCtrl.borderColor; opacity: 0.7
            }

            // ── TabBar ────────────────────────────────────────────────────────
            TabBar {
                id: tabBar
                Layout.fillWidth:  true
                Layout.topMargin:  4
                Layout.leftMargin: 4
                Layout.rightMargin: 4
                currentIndex: 0
                background: Rectangle { color: "transparent" }

                Repeater {
                    model: ["Índice", "Notas", "Marcadores"]
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

            // ── StackLayout ───────────────────────────────────────────────────
            StackLayout {
                Layout.fillWidth:  true
                Layout.fillHeight: true
                Layout.topMargin:  4
                currentIndex: tabBar.currentIndex

                // ── ABA 0: ÍNDICE (TOC real) ──────────────────────────────────
                // tocListModel é um QAbstractListModel registado como context
                // property no CasualModeWidget ("tocListModel").
                // Roles disponíveis: title (string), pageOrIndex (int),
                //                    depth (int), isCurrent (bool), url (string).
                //
                // Se o modelo ainda não estiver registado, usa o fallback
                // de placeholder abaixo.
                ScrollView {
                    clip: true

                    ListView {
                        id: tocList
                        // tocListModel é injectado como context property
                        // (ver CasualModeWidget::setupContext())
                        model: typeof tocListModel !== "undefined"
                               ? tocListModel
                               : emptyTocModel
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

                    // Placeholder quando não há TOC
                    ListModel { id: emptyTocModel }

                    Item {
                        anchors.fill: parent
                        visible: tocList.count === 0 && casualCtrl.hasDocument

                        Text {
                            anchors.centerIn: parent
                            text: "Este documento\nnão tem índice"
                            color: casualCtrl.mutedColor
                            font { pixelSize: 12; family: "Georgia, serif" }
                            horizontalAlignment: Text.AlignHCenter
                            opacity: 0.6
                        }
                    }

                    // Mensagem quando não há documento
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

                // ── ABA 1: NOTAS ──────────────────────────────────────────────
                // TODO: ligar ao SidecarManager via context property "noteListModel"
                Item {
                    Text {
                        anchors.centerIn: parent
                        text: "As notas serão carregadas\ndo ficheiro sidecar .md"
                        color: casualCtrl.mutedColor
                        font { pixelSize: 12; family: "Georgia, serif" }
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }

                // ── ABA 2: MARCADORES ─────────────────────────────────────────
                // TODO: ligar ao BookmarkManager via context property "bookmarkListModel"
                Item {
                    Text {
                        anchors.centerIn: parent
                        text: "Os marcadores serão\ncarregados do sidecar"
                        color: casualCtrl.mutedColor
                        font { pixelSize: 12; family: "Georgia, serif" }
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.5
                    }
                }
            }
        }
    }

    // ── Componentes inline ────────────────────────────────────────────────────

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
                    text:  pageNum > 0 ? pageNum : ""
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
                    // EPUB: usa índice de capítulo; PDF: usa número de página
                    if (casualCtrl.chapterCount > 0)
                        casualCtrl.setCurrentChapterIndex(index)
                    else
                        casualCtrl.setCurrentPage(pageNum)
                }
            }
        }
    }
}
