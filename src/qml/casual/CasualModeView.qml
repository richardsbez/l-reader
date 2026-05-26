// ─────────────────────────────────────────────────────────────────────────────
// CasualModeView.qml  —  l-reader · Modo Casual
//
// Layout de duas páginas lado a lado (book spread), paginação horizontal.
// Inspirado no Kindle Cloud Reader / Apple Books desktop.
//
// ┌──────────────────────────────────────────────────────────────────────┐
// │  Título obra          │         Capítulo X                          │  ← header 32px
// ├──────────────────────────────────────────────────────────────────────┤
// │                       │                                             │
// │   Página esquerda     │   Página direita                           │
// │   (texto coluna 1)    │   (texto coluna 2)                         │
// │                       │                                             │
// │                       │                                             │
// ├──────────────────────────────────────────────────────────────────────┤
// │  ‹  Loc. 30 of 527    │████████░░░░░░░░│    Loc. 31 of 527  ›     │  ← footer 32px
// └──────────────────────────────────────────────────────────────────────┘
//
// Navegação:
//   • Click na metade esquerda / tecla ← / botão ‹  → página anterior
//   • Click na metade direita  / tecla → / botão ›  → página seguinte
//   • Swipe horizontal com touchpad/touch
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    implicitWidth:  900
    implicitHeight: 600

    // ── Fundo geral (cor do tema) ─────────────────────────────────────────────
    Rectangle {
        id: background
        anchors.fill: parent
        color: casualCtrl.bgColor
        Behavior on color { ColorAnimation { duration: 220; easing.type: Easing.OutQuad } }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Sidebar QWidget (gerida pelo C++) — este QML não a controla.
    // CasualModeSidebar.qml mantido apenas para compatibilidade de imports.
    // ─────────────────────────────────────────────────────────────────────────

    // ─────────────────────────────────────────────────────────────────────────
    // ÁREA DE CONTEÚDO PRINCIPAL (ocupa tudo — sidebar QWidget fica sobre)
    // ─────────────────────────────────────────────────────────────────────────
    Item {
        id: contentArea
        anchors.fill: parent

        // ── Header ─────────────────────────────────────────────────────────
        CasualModeHeader {
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right }
        }

        Rectangle {
            id: headerDivider
            anchors { top: header.bottom; left: parent.left; right: parent.right }
            height: 1
            color:  casualCtrl.borderColor
            opacity: 0.5
        }

        // ── Área de leitura (book spread) ────────────────────────────────────
        Item {
            id: readingArea
            anchors {
                top:    headerDivider.bottom
                bottom: footerDivider.top
                left:   parent.left
                right:  parent.right
            }

            // ── Placeholder ───────────────────────────────────────────────────
            Item {
                anchors.fill: parent
                visible: !casualCtrl.hasDocument

                Column {
                    anchors.centerIn: parent
                    spacing: 10

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "📖"
                        font.pixelSize: 48
                        opacity: 0.2
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Abra um documento para começar a ler"
                        font.pixelSize: 14
                        color: casualCtrl.mutedColor
                        opacity: 0.6
                    }
                }
            }

            // ── Book spread: duas colunas de texto ────────────────────────────
            Item {
                id: spread
                anchors.fill: parent
                visible: casualCtrl.hasDocument

                // Gutter (linha central separando as duas páginas)
                Rectangle {
                    id: gutter
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top:    parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    color: casualCtrl.borderColor
                    opacity: 0.25
                }

                // ── Sombra interna das páginas (borda sutil) ──────────────────
                // Página esquerda
                Rectangle {
                    anchors {
                        top: parent.top; bottom: parent.bottom
                        left: parent.left; right: gutter.left
                    }
                    color: casualCtrl.bgColor
                    Behavior on color { ColorAnimation { duration: 220 } }

                    // Texto da página esquerda
                    Flickable {
                        id: leftPage
                        anchors {
                            fill: parent
                            leftMargin:  pageMargin
                            rightMargin: pageMargin / 2
                            topMargin:   pageTopMargin
                            bottomMargin: pageTopMargin
                        }
                        clip: true
                        interactive: false  // paginação por clique, não scroll
                        contentWidth:  width
                        contentHeight: leftText.implicitHeight

                        readonly property int pageMargin:    Math.max(32, parent.width * 0.08)
                        readonly property int pageTopMargin: 28

                        Text {
                            id: leftText
                            width: parent.width
                            text: casualCtrl.chapterHtml.length > 0 ? casualCtrl.chapterHtml : ""
                            textFormat: Text.RichText
                            wrapMode:   Text.WrapAtWordBoundaryOrAnywhere
                            horizontalAlignment: Text.AlignJustify
                            font.family:    "Georgia, 'Times New Roman', serif"
                            font.pixelSize: casualCtrl.fontSize
                            lineHeight:     casualCtrl.fontSize * 1.75 + casualCtrl.lineSpacing
                            lineHeightMode: Text.FixedHeight
                            color: casualCtrl.textColor

                            Behavior on color          { ColorAnimation { duration: 220 } }
                            Behavior on font.pixelSize { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                        }
                    }

                    // Número de página esquerda (canto inferior)
                    Text {
                        anchors {
                            bottom: parent.bottom
                            horizontalCenter: parent.horizontalCenter
                            bottomMargin: 8
                        }
                        text: casualCtrl.totalPages > 0
                              ? "Loc. %1 of %2".arg(casualCtrl.currentPage * 2 + 1)
                                               .arg(casualCtrl.totalPages)
                              : ""
                        color: casualCtrl.mutedColor
                        font.pixelSize: 11
                        font.family: "Georgia, serif"
                        opacity: 0.7
                    }
                }

                // Página direita
                Rectangle {
                    anchors {
                        top: parent.top; bottom: parent.bottom
                        left: gutter.right; right: parent.right
                    }
                    color: casualCtrl.bgColor
                    Behavior on color { ColorAnimation { duration: 220 } }

                    // Texto da página direita — offset para continuar da esquerda
                    Flickable {
                        id: rightPage
                        anchors {
                            fill: parent
                            leftMargin:  pageMargin / 2
                            rightMargin: pageMargin
                            topMargin:   pageTopMargin
                            bottomMargin: pageTopMargin
                        }
                        clip: true
                        interactive: false
                        contentWidth:  width
                        contentHeight: rightText.implicitHeight

                        // O conteúdo da direita é o mesmo HTML mas com offset vertical
                        // equivalente à altura de uma "página" da coluna esquerda.
                        contentY: leftPage.height > 0 ? leftPage.height : 0

                        readonly property int pageMargin:    Math.max(32, parent.width * 0.08)
                        readonly property int pageTopMargin: 28

                        Text {
                            id: rightText
                            width: parent.width
                            text: casualCtrl.chapterHtml.length > 0 ? casualCtrl.chapterHtml : ""
                            textFormat: Text.RichText
                            wrapMode:   Text.WrapAtWordBoundaryOrAnywhere
                            horizontalAlignment: Text.AlignJustify
                            font.family:    "Georgia, 'Times New Roman', serif"
                            font.pixelSize: casualCtrl.fontSize
                            lineHeight:     casualCtrl.fontSize * 1.75 + casualCtrl.lineSpacing
                            lineHeightMode: Text.FixedHeight
                            color: casualCtrl.textColor

                            Behavior on color          { ColorAnimation { duration: 220 } }
                            Behavior on font.pixelSize { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                        }
                    }

                    // Número de página direita
                    Text {
                        anchors {
                            bottom: parent.bottom
                            horizontalCenter: parent.horizontalCenter
                            bottomMargin: 8
                        }
                        text: casualCtrl.totalPages > 0
                              ? "Loc. %1 of %2".arg(casualCtrl.currentPage * 2 + 2)
                                               .arg(casualCtrl.totalPages)
                              : ""
                        color: casualCtrl.mutedColor
                        font.pixelSize: 11
                        font.family: "Georgia, serif"
                        opacity: 0.7
                    }
                }

                // ── Zonas de clique para virar página ─────────────────────────
                // Esquerda: página anterior (⅓ esquerdo)
                MouseArea {
                    id: prevClickZone
                    anchors {
                        top: parent.top; bottom: parent.bottom
                        left: parent.left
                    }
                    width: parent.width / 3
                    cursorShape: Qt.PointingHandCursor
                    enabled: casualCtrl.currentPage > 0
                    onClicked: {
                        pageFlipAnim.direction = "prev"
                        pageFlipAnim.restart()
                        casualCtrl.requestPrevChapter()
                    }
                }

                // Direita: página seguinte (⅓ direito)
                MouseArea {
                    id: nextClickZone
                    anchors {
                        top: parent.top; bottom: parent.bottom
                        right: parent.right
                    }
                    width: parent.width / 3
                    cursorShape: Qt.PointingHandCursor
                    enabled: casualCtrl.currentPage < casualCtrl.totalPages - 1
                    onClicked: {
                        pageFlipAnim.direction = "next"
                        pageFlipAnim.restart()
                        casualCtrl.requestNextChapter()
                    }
                }

                // ── Animação de virada de página ──────────────────────────────
                SequentialAnimation {
                    id: pageFlipAnim
                    property string direction: "next"

                    NumberAnimation {
                        target: spread
                        property: "opacity"
                        to: 0.0
                        duration: 120
                        easing.type: Easing.InQuad
                    }
                    NumberAnimation {
                        target: spread
                        property: "opacity"
                        to: 1.0
                        duration: 160
                        easing.type: Easing.OutQuad
                    }
                }

                // Reset do scroll ao mudar capítulo
                Connections {
                    target: casualCtrl
                    function onChapterChanged() {
                        leftPage.contentY  = 0
                        rightPage.contentY = leftPage.height > 0 ? leftPage.height : 0
                        pageFlipAnim.restart()
                    }
                }

                // ── Swipe horizontal (trackpad / touch) ───────────────────────
                SwipeView {
                    id: swipeDetector
                    anchors.fill: parent
                    interactive: true
                    opacity: 0  // invisível — apenas detecta gestos
                    currentIndex: 1  // posição central

                    Item {}  // página "anterior" (dummy)
                    Item {}  // página "actual"
                    Item {}  // página "seguinte" (dummy)

                    onCurrentIndexChanged: {
                        if (currentIndex === 0) {
                            casualCtrl.requestPrevChapter()
                            currentIndex = 1
                        } else if (currentIndex === 2) {
                            casualCtrl.requestNextChapter()
                            currentIndex = 1
                        }
                    }
                }
            }
        }

        Rectangle {
            id: footerDivider
            anchors { bottom: footer.top; left: parent.left; right: parent.right }
            height: 1
            color: casualCtrl.borderColor
            opacity: 0.5
        }

        // ── Footer ─────────────────────────────────────────────────────────
        CasualModeFooter {
            id: footer
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        }
    }
}
