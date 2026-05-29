// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/casual_mode_footer.qml  —  l-reader · Modo Casual
//
// Mudanças em relação à versão anterior:
//   • chapterBreaks substituído por casualCtrl.chapterBreakPositions (dados reais)
//   • Botões ← → de navegação de capítulo conectados às invokables reais
//   • Labels "Página X de Y" e "%" já usavam propriedades reactivas — sem mudança
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    height: 20

    Rectangle {
        anchors.fill: parent
        color: casualCtrl.headerBg

        Behavior on color { ColorAnimation { duration: 220 } }

        ColumnLayout {
            anchors {
                fill:         parent
                leftMargin:   250 + 16
                rightMargin:  16
                topMargin:    6
                bottomMargin: 4
            }
            spacing: 5

            // ── Linha superior: info de página + título de capítulo + % ────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                // ← Capítulo anterior
                Rectangle {
                    visible: casualCtrl.chapterCount > 0
                    width: 20; height: 16; radius: 3
                    color: prevArea.containsMouse
                               ? Qt.alpha(casualCtrl.textColor, 0.08)
                               : "transparent"

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text:  "‹"
                        color: casualCtrl.currentPage <= 0
                                   ? Qt.alpha(casualCtrl.mutedColor, 0.3)
                                   : casualCtrl.mutedColor
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: prevArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        enabled:      casualCtrl.currentPage > 0
                        onClicked:    casualCtrl.requestPrevChapter()
                    }
                }

                // Informação de página — esquerda
                Text {
                    text: {
                        if (!casualCtrl.hasDocument) return ""
                        return "Loc. %1 of %2".arg(casualCtrl.currentPage * 2 + 1)
                                              .arg(casualCtrl.totalPages)
                    }
                    color: casualCtrl.mutedColor
                    font { pixelSize: 11; family: "Georgia, serif"; letterSpacing: 0.2 }
                    leftPadding: casualCtrl.chapterCount > 0 ? 4 : 0
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                Item { Layout.fillWidth: true }

                // Título do capítulo actual (centro — visível ao hover na barra)
                Text {
                    id: chapterHint
                    text:  casualCtrl.chapterTitle
                    color: Qt.alpha(casualCtrl.mutedColor, progressArea.containsMouse ? 0.8 : 0.0)
                    font.pixelSize: 10
                    font.family:    "Georgia, serif"
                    elide:          Text.ElideMiddle
                    maximumLineCount: 1

                    Behavior on color { ColorAnimation { duration: 180 } }
                }

                Item { Layout.fillWidth: true }

                // Percentagem / Loc. direita
                Text {
                    text: casualCtrl.hasDocument
                          ? "Loc. %1 of %2".arg(casualCtrl.currentPage * 2 + 2)
                                           .arg(casualCtrl.totalPages)
                          : ""
                    color: casualCtrl.mutedColor
                    font.pixelSize: 11
                    font.family:    "Georgia, serif"
                    rightPadding:   casualCtrl.chapterCount > 0 ? 4 : 0
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                // → Capítulo seguinte
                Rectangle {
                    visible: casualCtrl.chapterCount > 0
                    width: 20; height: 16; radius: 3
                    color: nextArea.containsMouse
                               ? Qt.alpha(casualCtrl.textColor, 0.08)
                               : "transparent"

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text:  "›"
                        color: casualCtrl.currentPage >= casualCtrl.totalPages - 1
                                   ? Qt.alpha(casualCtrl.mutedColor, 0.3)
                                   : casualCtrl.mutedColor
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: nextArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        enabled:      casualCtrl.currentPage < casualCtrl.totalPages - 1
                        onClicked:    casualCtrl.requestNextChapter()
                    }
                }
            }

            // ── Barra de progresso com ticks de capítulo ──────────────────────
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

                // Preenchimento
                Rectangle {
                    id: fill
                    anchors { left: track.left; top: track.top; bottom: track.bottom }
                    width:  track.width * casualCtrl.readingProgress
                    radius: 2
                    color:  casualCtrl.accentColor

                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }
                    Behavior on color { ColorAnimation  { duration: 200 } }
                }

                // ── Ticks de capítulo (dados reais do controller) ─────────────
                // casualCtrl.chapterBreakPositions é um QVariantList<qreal> 0..1
                // calculado em CasualModeController::computeChapterBreaks()
                // a partir do TOC real do documento.
                Repeater {
                    model: casualCtrl.chapterBreakPositions

                    Rectangle {
                        required property real modelData
                        required property int  index

                        visible: modelData > 0.001 && modelData < 0.999

                        x:      track.width * modelData - 1
                        y:      -1
                        width:  2
                        height: progressBar.height + 2
                        radius: 1

                        color: modelData <= casualCtrl.readingProgress
                                   ? Qt.lighter(casualCtrl.accentColor, 1.35)
                                   : Qt.alpha(casualCtrl.textColor, 0.25)

                        ToolTip.visible: tickArea.containsMouse
                        ToolTip.text:    "Cap. " + (index + 1)
                        ToolTip.delay:   500

                        Behavior on color { ColorAnimation { duration: 200 } }

                        MouseArea {
                            id:          tickArea
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled:    true
                            // Clicar num tick salta para esse capítulo
                            onClicked: casualCtrl.setCurrentChapterIndex(index)
                        }
                    }
                }

                // Handle (bolinha na ponta do fill)
                Rectangle {
                    width:  10; height: 10; radius: 5
                    color:  casualCtrl.accentColor
                    border.color: casualCtrl.headerBg
                    border.width: 1.5

                    x: fill.width - width / 2
                    anchors.verticalCenter: parent.verticalCenter

                    opacity: progressArea.containsMouse ? 1.0 : 0.0
                    scale:   progressArea.containsMouse ? 1.0 : 0.5

                    Behavior on opacity { NumberAnimation { duration: 150 } }
                    Behavior on scale   { NumberAnimation { duration: 150 } }
                    Behavior on color   { ColorAnimation  { duration: 200 } }
                    Behavior on x       { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }
                }

                // Área de interacção — clicar para saltar
                MouseArea {
                    id: progressArea
                    anchors.fill: track
                    anchors.topMargin:    -8
                    anchors.bottomMargin: -8
                    hoverEnabled:  true
                    cursorShape:   Qt.PointingHandCursor

                    onClicked: function(mouse) {
                        const ratio = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                        casualCtrl.setCurrentPage(Math.round(ratio * (casualCtrl.totalPages - 1)))
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed) return
                        const ratio = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                        casualCtrl.setCurrentPage(Math.round(ratio * (casualCtrl.totalPages - 1)))
                    }
                }
            }
        }
    }
}
