// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/CasualModeView.qml  —  l-reader · Modo Casual
//
// View raiz do Modo Casual.  Orquestra o Header, a área de leitura bicolumnar,
// o Footer e a Sidebar, respondendo reactivamente às propriedades do controller.
//
// Dependências QML:
//   QtQuick 6.4+, QtQuick.Controls 2 (Basic style)
// Dependências C++:
//   casualCtrl  →  CasualModeController (registado como context property)
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    // ── Dimensões mínimas recomendadas ────────────────────────────────────────
    implicitWidth:  900
    implicitHeight: 600

    // ── Fundo — actualizado pelo tema via Behavior ────────────────────────────
    Rectangle {
        id: background
        anchors.fill: parent
        color: casualCtrl.bgColor

        Behavior on color {
            ColorAnimation { duration: 220; easing.type: Easing.OutQuad }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Sidebar — desliza da esquerda com animação suave
    // ─────────────────────────────────────────────────────────────────────────
    CasualModeSidebar {
        id: sidebar
        anchors {
            top:    parent.top
            bottom: parent.bottom
            left:   parent.left
        }
        // width é controlado internamente pelo próprio componente
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Área de conteúdo principal — desloca-se à direita da sidebar
    // ─────────────────────────────────────────────────────────────────────────
    Item {
        id: contentArea
        anchors {
            top:    parent.top
            bottom: parent.bottom
            left:   sidebar.right
            right:  parent.right
        }

        // Transição suave quando a sidebar abre/fecha
        Behavior on anchors.leftMargin {
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }

        // ── Header ────────────────────────────────────────────────────────────
        CasualModeHeader {
            id: header
            anchors {
                top:   parent.top
                left:  parent.left
                right: parent.right
            }
        }

        // ── Separador do header ───────────────────────────────────────────────
        Rectangle {
            id: headerDivider
            anchors {
                top:   header.bottom
                left:  parent.left
                right: parent.right
            }
            height: 1
            color:  casualCtrl.borderColor
            opacity: 0.6
        }

        // ── Área de leitura bicolumnar — livro aberto ─────────────────────────
        Item {
            id: readingArea
            anchors {
                top:    headerDivider.bottom
                bottom: footerDivider.top
                left:   parent.left
                right:  parent.right
            }

            // Goteira central (gutter) entre as colunas
            readonly property int gutterWidth:   32
            // Margem lateral externa de cada coluna (reactiva ao controlador)
            readonly property int outerMargin:   casualCtrl.columnMargin
            // Largura de cada coluna: metade do espaço disponível menos margens
            readonly property int columnWidth: Math.floor(
                (width - 2 * outerMargin - gutterWidth) / 2
            )

            // ── Linha divisória central (goteira visual) ──────────────────────
            Rectangle {
                id: columnDivider
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top:    parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: casualCtrl.borderColor
                opacity: 0.35
            }

            // ── Coluna esquerda (página par) ──────────────────────────────────
            Flickable {
                id: leftFlickable
                anchors {
                    top:    parent.top
                    bottom: parent.bottom
                    left:   parent.left
                    right:  columnDivider.left
                }
                contentWidth:  width
                contentHeight: leftText.height + 64

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

                Text {
                    id: leftText
                    width: readingArea.columnWidth
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 40

                    text:            casualCtrl.leftPageHtml
                    textFormat:      Text.RichText
                    wrapMode:        Text.WrapAtWordBoundaryOrAnywhere
                    horizontalAlignment: Text.AlignJustify

                    // Tipografia — fonte serifada elegante
                    font.family:     "Georgia, 'Times New Roman', serif"
                    font.pixelSize:  casualCtrl.fontSize
                    lineSpacing:     casualCtrl.lineSpacing + 6
                    lineHeightMode:  Text.FixedHeight
                    lineHeight:      casualCtrl.fontSize * 1.75 + casualCtrl.lineSpacing

                    color:           casualCtrl.textColor

                    // Suaviza a transição de cor do tema
                    Behavior on color {
                        ColorAnimation { duration: 220 }
                    }
                    Behavior on font.pixelSize {
                        NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                    }
                }
            }

            // ── Coluna direita (página ímpar) ─────────────────────────────────
            Flickable {
                id: rightFlickable
                anchors {
                    top:    parent.top
                    bottom: parent.bottom
                    left:   columnDivider.right
                    right:  parent.right
                }
                contentWidth:  width
                contentHeight: rightText.height + 64

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

                Text {
                    id: rightText
                    width: readingArea.columnWidth
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 40

                    text:            casualCtrl.rightPageHtml
                    textFormat:      Text.RichText
                    wrapMode:        Text.WrapAtWordBoundaryOrAnywhere
                    horizontalAlignment: Text.AlignJustify

                    font.family:     "Georgia, 'Times New Roman', serif"
                    font.pixelSize:  casualCtrl.fontSize
                    lineSpacing:     casualCtrl.lineSpacing + 6
                    lineHeightMode:  Text.FixedHeight
                    lineHeight:      casualCtrl.fontSize * 1.75 + casualCtrl.lineSpacing

                    color:           casualCtrl.textColor

                    Behavior on color {
                        ColorAnimation { duration: 220 }
                    }
                    Behavior on font.pixelSize {
                        NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                    }
                }
            }
        }

        // ── Separador do footer ───────────────────────────────────────────────
        Rectangle {
            id: footerDivider
            anchors {
                bottom: footer.top
                left:   parent.left
                right:  parent.right
            }
            height: 1
            color:  casualCtrl.borderColor
            opacity: 0.6
        }

        // ── Footer — timeline de progresso ────────────────────────────────────
        CasualModeFooter {
            id: footer
            anchors {
                bottom: parent.bottom
                left:   parent.left
                right:  parent.right
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Overlay de sombra — aparece quando a sidebar está aberta (mobile feel)
    // ─────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: sidebarOverlay
        anchors.fill: parent
        color: "#000000"
        opacity: casualCtrl.sidebarOpen ? 0.22 : 0.0
        visible: opacity > 0.0

        Behavior on opacity {
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }

        // Clicar no overlay fecha a sidebar
        MouseArea {
            anchors.fill: parent
            onClicked: casualCtrl.setSidebarOpen(false)
            cursorShape: Qt.ArrowCursor
        }
    }
}
