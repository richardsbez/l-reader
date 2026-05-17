// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/CasualModeFooter.qml  —  l-reader · Modo Casual
//
// Rodapé minimalista: timeline de progresso de leitura + info de página.
//
// Layout:
//   [ Página 12 de 300 ]  ─────────────|──────────────  [ 4% ]
//   (abaixo: slider ultra-fino com ticks de capítulos)
//
// Altura total: 32 px
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    height: 32

    // Divisões mockadas de capítulos (como proporção 0..1 do total de páginas)
    readonly property var chapterBreaks: [
        0.0,   // Cap. I
        0.09,  // Cap. II
        0.19,  // Cap. III  ← posição actual (4% dentro deste cap.)
        0.31,  // Cap. IV
        0.44,  // Cap. V
        0.57,  // Cap. VI
        0.68,  // Cap. VII
        0.79,  // Cap. VIII
        0.89,  // Cap. IX
        1.0    // Fim
    ]

    Rectangle {
        anchors.fill: parent
        color: casualCtrl.headerBg

        Behavior on color {
            ColorAnimation { duration: 220 }
        }

        ColumnLayout {
            anchors {
                fill:          parent
                leftMargin:    16
                rightMargin:   16
                topMargin:     6
                bottomMargin:  4
            }
            spacing: 5

            // ── Linha superior: texto de progresso + percentagem ──────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                // Informação de página (esquerda)
                Text {
                    text:  "Página %1 de %2".arg(casualCtrl.currentPage)
                                            .arg(casualCtrl.totalPages)
                    color: casualCtrl.mutedColor
                    font {
                        pixelSize:   11
                        family:      "Georgia, serif"
                        letterSpacing: 0.2
                    }

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                Item { Layout.fillWidth: true }

                // Capítulo actual (centro — aparece ao passar o rato na barra)
                Text {
                    id: chapterHint
                    text:  casualCtrl.chapterTitle
                    color: Qt.alpha(casualCtrl.mutedColor, progressArea.containsMouse ? 0.8 : 0.0)
                    font.pixelSize: 10
                    font.family:    "Georgia, serif"

                    Behavior on color { ColorAnimation { duration: 180 } }
                }

                Item { Layout.fillWidth: true }

                // Percentagem (direita)
                Text {
                    text:  "%1%".arg(Math.round(casualCtrl.readingProgress * 100))
                    color: casualCtrl.mutedColor
                    font.pixelSize: 11
                    font.family:    "Georgia, serif"

                    Behavior on color { ColorAnimation { duration: 200 } }
                }
            }

            // ── Linha inferior: barra de progresso com ticks ──────────────────
            Item {
                id: progressBar
                Layout.fillWidth: true
                height: 4

                // Trilho de fundo
                Rectangle {
                    id: track
                    anchors.fill: parent
                    radius: 2
                    color:  casualCtrl.borderColor

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                // Preenchimento de progresso
                Rectangle {
                    id: fill
                    anchors {
                        left:           track.left
                        top:            track.top
                        bottom:         track.bottom
                    }
                    width:  track.width * casualCtrl.readingProgress
                    radius: 2
                    color:  casualCtrl.accentColor

                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                // Marcações de capítulo (ticks)
                Repeater {
                    model: root.chapterBreaks

                    Rectangle {
                        required property real modelData
                        required property int  index

                        // Não desenha o tick na posição 0 nem na posição 1 (bordas)
                        visible: modelData > 0.001 && modelData < 0.999

                        x:      track.width * modelData - 1
                        y:      -1
                        width:  2
                        height: progressBar.height + 2
                        radius: 1

                        // Tick à esquerda do progresso: usa cor do fill; à direita: cor do track
                        color: modelData <= casualCtrl.readingProgress
                                   ? Qt.lighter(casualCtrl.accentColor, 1.35)
                                   : Qt.alpha(casualCtrl.textColor, 0.25)

                        // Tooltip com número do capítulo
                        ToolTip.visible:  tickArea.containsMouse
                        ToolTip.text:     "Cap. " + (index + 1)
                        ToolTip.delay:    500

                        Behavior on color { ColorAnimation { duration: 200 } }

                        MouseArea {
                            id:          tickArea
                            anchors.fill: parent
                            // Área de hit aumentada para facilitar a interacção
                            anchors.margins: -6
                            hoverEnabled:    true
                        }
                    }
                }

                // Handle de posição actual (bolinha na ponta do fill)
                Rectangle {
                    id: progressHandle
                    width:  10
                    height: 10
                    radius: 5
                    color:  casualCtrl.accentColor
                    border.color: casualCtrl.headerBg
                    border.width: 1.5

                    x: fill.width - width / 2
                    anchors.verticalCenter: parent.verticalCenter

                    // Só visível quando o rato está sobre a barra
                    opacity: progressArea.containsMouse ? 1.0 : 0.0
                    scale:   progressArea.containsMouse ? 1.0 : 0.5

                    Behavior on opacity { NumberAnimation { duration: 150 } }
                    Behavior on scale   { NumberAnimation { duration: 150 } }
                    Behavior on color   { ColorAnimation  { duration: 200 } }
                    Behavior on x       { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }
                }

                // Área de interacção da barra (clicar para saltar para posição)
                MouseArea {
                    id:          progressArea
                    anchors.fill: track
                    // Aumenta área de hit verticalmente
                    anchors.topMargin:    -8
                    anchors.bottomMargin: -8
                    hoverEnabled:         true
                    cursorShape:          Qt.PointingHandCursor

                    onClicked: function(mouse) {
                        const ratio = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                        casualCtrl.setCurrentPage(Math.round(ratio * casualCtrl.totalPages))
                    }

                    // Preview da posição ao passar o rato
                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            const ratio = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                            casualCtrl.setCurrentPage(Math.round(ratio * casualCtrl.totalPages))
                        }
                    }
                }
            }
        }
    }
}
