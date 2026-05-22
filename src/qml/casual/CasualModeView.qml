// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/CasualModeView.qml  —  l-reader · Modo Casual
//
// Mudanças em relação à versão anterior:
//   • leftText.text usa casualCtrl.chapterHtml (HTML real do documento)
//   • rightText removido — conteúdo completo numa única coluna scrollável
//   • columnDivider removido
//   • Placeholder "Abra um documento..." mostrado quando !casualCtrl.hasDocument
//   • Botões de capítulo anterior/próximo no footer usam as invokables reais
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    implicitWidth:  900
    implicitHeight: 600

    // ── Fundo ─────────────────────────────────────────────────────────────────
    Rectangle {
        id: background
        anchors.fill: parent
        color: casualCtrl.bgColor

        Behavior on color {
            ColorAnimation { duration: 220; easing.type: Easing.OutQuad }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Sidebar
    // ─────────────────────────────────────────────────────────────────────────
    CasualModeSidebar {
        id: sidebar
        anchors {
            top:    parent.top
            bottom: parent.bottom
            left:   parent.left
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Área de conteúdo principal
    // ─────────────────────────────────────────────────────────────────────────
    Item {
        id: contentArea
        anchors {
            top:    parent.top
            bottom: parent.bottom
            left:   sidebar.right
            right:  parent.right
        }

        // ── Header ────────────────────────────────────────────────────────────
        CasualModeHeader {
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right }
        }

        Rectangle {
            id: headerDivider
            anchors { top: header.bottom; left: parent.left; right: parent.right }
            height: 1
            color:  casualCtrl.borderColor
            opacity: 0.6
        }

        // ── Área de leitura ───────────────────────────────────────────────────
        Item {
            id: readingArea
            anchors {
                top:    headerDivider.bottom
                bottom: footerDivider.top
                left:   parent.left
                right:  parent.right
            }

            // ── Placeholder quando não há documento aberto ────────────────────
            Item {
                anchors.fill: parent
                visible: !casualCtrl.hasDocument

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "📖"
                        font.pixelSize: 40
                        opacity: 0.25
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Abra um documento para começar a ler"
                        font.pixelSize: 14
                        color: casualCtrl.mutedColor
                        opacity: 0.7
                    }
                }
            }

            // ── Conteúdo real ─────────────────────────────────────────────────
            // Única coluna scrollável com o HTML do capítulo actual.
            // A largura máxima do texto segue casualCtrl.columnMargin para
            // manter as linhas confortáveis (60-75 caracteres).
            Flickable {
                id:      chapterFlickable
                visible: casualCtrl.hasDocument
                anchors {
                    fill:        parent
                    topMargin:   0
                    bottomMargin: 0
                }
                contentWidth:  width
                contentHeight: chapterText.implicitHeight + 80

                // Sincroniza o progresso de leitura com a posição do scroll.
                // Emite apenas quando o utilizador faz scroll manual (não
                // quando o controller muda a página programaticamente).
                onContentYChanged: {
                    if (!moving) return
                    const maxY = contentHeight - height
                    if (maxY <= 0) return
                    // O progresso do controller já considera a página; aqui
                    // refinamos dentro do capítulo (contribuição do scroll).
                    // Mantemos simples: não interferimos com setCurrentPage().
                }

                // Reset ao scroll ao mudar de capítulo
                Connections {
                    target: casualCtrl
                    function onChapterChanged() {
                        chapterFlickable.contentY = 0
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    opacity: 0.4
                }

                // ── Texto do capítulo ─────────────────────────────────────────
                Text {
                    id: chapterText

                    // Margem lateral reactiva ao controlador (estilo e-reader)
                    x:     casualCtrl.columnMargin
                    width: parent.width - 2 * casualCtrl.columnMargin
                    y:     40

                    // Conteúdo HTML real (body do ficheiro EPUB)
                    // Para PDF, chapterHtml fica vazio — o PdfCanvasView
                    // é mantido no stack central; este widget não é mostrado.
                    text:       casualCtrl.chapterHtml.length > 0
                                    ? casualCtrl.chapterHtml
                                    : ""
                    textFormat: Text.RichText
                    wrapMode:   Text.WrapAtWordBoundaryOrAnywhere
                    horizontalAlignment: Text.AlignJustify

                    // Tipografia
                    font.family:    "Georgia, 'Times New Roman', serif"
                    font.pixelSize: casualCtrl.fontSize
                    lineHeight:     casualCtrl.fontSize * 1.75 + casualCtrl.lineSpacing
                    lineHeightMode: Text.FixedHeight
                    color:          casualCtrl.textColor

                    Behavior on color {
                        ColorAnimation { duration: 220 }
                    }
                    Behavior on font.pixelSize {
                        NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                    }
                    Behavior on x {
                        NumberAnimation { duration: 160; easing.type: Easing.OutQuad }
                    }
                }
            }
        }

        Rectangle {
            id: footerDivider
            anchors { bottom: footer.top; left: parent.left; right: parent.right }
            height: 1
            color:  casualCtrl.borderColor
            opacity: 0.6
        }

        // ── Footer ────────────────────────────────────────────────────────────
        CasualModeFooter {
            id: footer
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        }
    }

    // ── Overlay de sombra da sidebar ──────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: casualCtrl.sidebarOpen ? 0.22 : 0.0
        visible: opacity > 0.0

        Behavior on opacity {
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: casualCtrl.setSidebarOpen(false)
            cursorShape: Qt.ArrowCursor
        }
    }
}
