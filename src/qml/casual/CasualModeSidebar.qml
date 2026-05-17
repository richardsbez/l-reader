// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/CasualModeSidebar.qml  —  l-reader · Modo Casual
//
// Sidebar ocultável com animação de deslize (slide-in/out).
// Contém três abas: Índice | Notas | Marcadores
//
// Integração: o estado de visibilidade é controlado por casualCtrl.sidebarOpen.
// A largura real é 260 px; quando fechada, x = -width.
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    // Largura fixa quando aberta
    readonly property int openWidth: 260
    // A largura do Item é sempre openWidth; o painel desliza para fora por x
    width: openWidth

    // ── Posição horizontal — animação de slide ────────────────────────────────
    Rectangle {
        id: panel
        width:  root.openWidth
        height: root.height
        color:  casualCtrl.headerBg
        // Sombra direita
        layer.enabled: true

        // Borda direita como separador
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: casualCtrl.borderColor

            Behavior on color { ColorAnimation { duration: 220 } }
        }

        // Posição: desliza para dentro/fora
        x: casualCtrl.sidebarOpen ? 0 : -root.openWidth

        Behavior on x {
            NumberAnimation {
                duration:  280
                easing.type: Easing.OutCubic
            }
        }
        Behavior on color {
            ColorAnimation { duration: 220 }
        }

        ColumnLayout {
            anchors {
                fill:          parent
                topMargin:     8
                bottomMargin:  8
                leftMargin:    0
                rightMargin:   0
            }
            spacing: 0

            // ── Cabeçalho da sidebar ──────────────────────────────────────────
            RowLayout {
                Layout.fillWidth:  true
                Layout.leftMargin:  16
                Layout.rightMargin: 12
                Layout.bottomMargin: 6
                spacing: 0

                // Título do livro
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text:  casualCtrl.bookTitle
                        color: casualCtrl.textColor
                        font {
                            pixelSize: 13
                            weight:    Font.SemiBold
                            family:    "Georgia, serif"
                        }
                        elide: Text.ElideRight
                        Layout.fillWidth: true

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                    Text {
                        text:  "Umberto Eco"
                        color: casualCtrl.mutedColor
                        font.pixelSize: 11
                        font.family:    "Georgia, serif"

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }

                // Botão fechar
                Rectangle {
                    width:  28; height: 28; radius: 6
                    color:  closeArea.containsMouse
                                ? Qt.alpha(casualCtrl.textColor, 0.08)
                                : "transparent"

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text:  "×"
                        color: casualCtrl.mutedColor
                        font.pixelSize: 16
                    }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        onClicked:    casualCtrl.setSidebarOpen(false)
                    }
                }
            }

            // ── Divisor ───────────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color:  casualCtrl.borderColor
                opacity: 0.7
            }

            // ── TabBar: Índice / Notas / Marcadores ───────────────────────────
            TabBar {
                id: tabBar
                Layout.fillWidth:  true
                Layout.topMargin:  4
                Layout.leftMargin: 4
                Layout.rightMargin: 4
                currentIndex: 0

                background: Rectangle {
                    color: "transparent"
                }

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
                                       ? casualCtrl.accentColor
                                       : casualCtrl.mutedColor
                            font.pixelSize: 11
                            font.weight:    tabBar.currentIndex === parent.index
                                                ? Font.SemiBold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            // Sublinhado na aba activa
                            Rectangle {
                                visible: tabBar.currentIndex === parent.index
                                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                                height: 2
                                radius: 1
                                color:  casualCtrl.accentColor
                            }
                            color: "transparent"
                        }
                    }
                }
            }

            // ── Conteúdo de cada aba (StackLayout) ───────────────────────────
            StackLayout {
                id: tabContent
                Layout.fillWidth:  true
                Layout.fillHeight: true
                Layout.topMargin:  4
                currentIndex: tabBar.currentIndex

                // ── ABA 0: ÍNDICE ─────────────────────────────────────────────
                ScrollView {
                    clip: true

                    ListView {
                        id: tocList
                        model: tocModel
                        spacing: 0

                        delegate: TocEntry {
                            required property string chapterName
                            required property int    chapterPage
                            required property int    depth
                            required property bool   isCurrent

                            label:       chapterName
                            pageNum:     chapterPage
                            indentLevel: depth
                            active:      isCurrent
                            width:       tocList.width
                        }
                    }

                    // Modelo mockado da TOC
                    ListModel {
                        id: tocModel
                        ListElement { chapterName: "Prólogo";                   chapterPage: 1;   depth: 0; isCurrent: false }
                        ListElement { chapterName: "Capítulo I — A Chegada";    chapterPage: 8;   depth: 0; isCurrent: false }
                        ListElement { chapterName: "Primeiro Dia — Hora Prima"; chapterPage: 9;   depth: 1; isCurrent: false }
                        ListElement { chapterName: "Primeiro Dia — Hora Terça"; chapterPage: 15;  depth: 1; isCurrent: false }
                        ListElement { chapterName: "Capítulo II — O Scriptorium"; chapterPage: 22; depth: 0; isCurrent: false }
                        ListElement { chapterName: "Capítulo III — O Labirinto"; chapterPage: 34; depth: 0; isCurrent: true  }
                        ListElement { chapterName: "Segundo Dia — Hora Prima";  chapterPage: 35;  depth: 1; isCurrent: false }
                        ListElement { chapterName: "Segundo Dia — Hora Sexta";  chapterPage: 48;  depth: 1; isCurrent: false }
                        ListElement { chapterName: "Capítulo IV — A Biblioteca"; chapterPage: 61; depth: 0; isCurrent: false }
                        ListElement { chapterName: "Capítulo V — O Veneno";     chapterPage: 89;  depth: 0; isCurrent: false }
                        ListElement { chapterName: "Capítulo VI — O Segredo";   chapterPage: 118; depth: 0; isCurrent: false }
                        ListElement { chapterName: "Epílogo";                   chapterPage: 280; depth: 0; isCurrent: false }
                    }
                }

                // ── ABA 1: NOTAS ──────────────────────────────────────────────
                Item {
                    ColumnLayout {
                        anchors {
                            fill:        parent
                            margins:     16
                        }
                        spacing: 12

                        // Nota mockada
                        NoteCard {
                            label:   "Cap. I, p. 9"
                            content: "A descrição do mosteiro remete para a abadia de Melk, na Áustria."
                            Layout.fillWidth: true
                        }
                        NoteCard {
                            label:   "Cap. II, p. 25"
                            content: "Guilherme usa métodos dedutivos avant la lettre — semelhante a Holmes."
                            Layout.fillWidth: true
                        }
                        NoteCard {
                            label:   "Cap. III, p. 37"
                            content: "O labirinto como metáfora do conhecimento proibido é central na obra."
                            Layout.fillWidth: true
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ── ABA 2: MARCADORES ─────────────────────────────────────────
                Item {
                    ColumnLayout {
                        anchors {
                            fill:        parent
                            margins:     16
                        }
                        spacing: 8

                        Repeater {
                            model: [
                                { page: 12,  label: "O encontro com Malaquias" },
                                { page: 34,  label: "Entrada no labirinto"     },
                                { page: 57,  label: "Diálogo sobre o riso"     },
                            ]
                            BookmarkEntry {
                                required property var modelData
                                pageNum: modelData.page
                                label:   modelData.label
                                Layout.fillWidth: true
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Componentes de conteúdo inline
    // ─────────────────────────────────────────────────────────────────────────────

    // Entrada do índice (TOC)
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
                       : (entryArea.containsMouse ? Qt.alpha(casualCtrl.textColor, 0.05) : "transparent")

            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors {
                    fill:        parent
                    leftMargin:  12 + indentLevel * 14
                    rightMargin: 12
                }
                spacing: 4

                // Indicador de capítulo actual
                Rectangle {
                    visible: active
                    width:   3; height: 16; radius: 1.5
                    color:   casualCtrl.accentColor
                }

                Text {
                    Layout.fillWidth: true
                    text:      label
                    elide:     Text.ElideRight
                    color:     active ? casualCtrl.accentColor : casualCtrl.textColor
                    font.pixelSize: indentLevel > 0 ? 11 : 12
                    font.weight:    active ? Font.Medium : Font.Normal
                    font.family:    "Georgia, serif"

                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                Text {
                    text:  pageNum > 0 ? pageNum : ""
                    color: casualCtrl.mutedColor
                    font.pixelSize: 10
                    font.family:    "Georgia, serif"
                }
            }

            MouseArea {
                id: entryArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked:    casualCtrl.setCurrentPage(pageNum)
            }
        }
    }

    // Cartão de nota
    component NoteCard: Rectangle {
        property string label:   ""
        property string content: ""

        height: cardCol.implicitHeight + 16
        color:  Qt.alpha(casualCtrl.textColor, 0.05)
        radius: 8
        border.color: casualCtrl.borderColor
        border.width: 1

        Behavior on color { ColorAnimation { duration: 200 } }

        ColumnLayout {
            id: cardCol
            anchors {
                fill:    parent
                margins: 10
            }
            spacing: 3

            Text {
                text:  label
                color: casualCtrl.accentColor
                font { pixelSize: 10; weight: Font.Medium; family: "Georgia, serif" }
            }
            Text {
                Layout.fillWidth: true
                text:      content
                color:     casualCtrl.textColor
                font { pixelSize: 11; family: "Georgia, serif" }
                wrapMode:  Text.WordWrap
                opacity:   0.85
            }
        }
    }

    // Entrada de marcador
    component BookmarkEntry: Rectangle {
        property int    pageNum: 0
        property string label:   ""

        height: 38
        color:  bmArea.containsMouse ? Qt.alpha(casualCtrl.textColor, 0.05) : "transparent"
        radius: 8

        Behavior on color { ColorAnimation { duration: 100 } }

        RowLayout {
            anchors { fill: parent; leftMargin: 4; rightMargin: 4 }
            spacing: 8

            // Ícone de marcador (bookmark shape em texto)
            Text {
                text:  "🔖"
                font.pixelSize: 13
            }

            Text {
                Layout.fillWidth: true
                text:      label
                color:     casualCtrl.textColor
                font { pixelSize: 12; family: "Georgia, serif" }
                elide:     Text.ElideRight
            }

            Text {
                text:  "p. " + pageNum
                color: casualCtrl.mutedColor
                font.pixelSize: 10
            }
        }

        MouseArea {
            id: bmArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    casualCtrl.setCurrentPage(pageNum)
        }
    }
}
