// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/casual_mode_footer.qml  —  l-reader · Modo Casual
//
// Redesign v2 — barra de progresso editorial aprimorada:
//
//   ‹  [6%]  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  [p. 3 / 52]  ›
//
// Melhorias v2:
//   • Altura 44px (era 36px) — mais respiro visual
//   • Track 4px → 6px no hover com gradiente de brilho no fill
//   • Handle com anel de pulso ao arrastar (spring + glow)
//   • Ticks de capítulo com label flutuante melhorado
//   • Tooltip de scrub posicionado acima do handle (não do cursor)
//   • Botões ‹/› com fundo pill no hover, ícone ativo/inativo animado
//   • Separador superior com degradê — not full-opacity line
//   • Percentagem e localização com peso tipográfico diferenciado
//   • Todos os Behaviors reduzidos para resposta mais rápida
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    height: 44

    // ── Fundo com borda superior em gradiente ────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: casualCtrl.headerBg
        Behavior on color { ColorAnimation { duration: 180 } }

        // Borda superior: gradiente horizontal — mais intensa no centro
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0;  color: "transparent" }
                GradientStop { position: 0.15; color: Qt.alpha(casualCtrl.borderColor, 0.55) }
                GradientStop { position: 0.5;  color: Qt.alpha(casualCtrl.borderColor, 0.70) }
                GradientStop { position: 0.85; color: Qt.alpha(casualCtrl.borderColor, 0.55) }
                GradientStop { position: 1.0;  color: "transparent" }
            }
            Behavior on color { ColorAnimation { duration: 180 } }
        }

        // ── Layout principal ─────────────────────────────────────────────────
        RowLayout {
            anchors {
                fill: parent
                leftMargin:  14
                rightMargin: 14
            }
            spacing: 0

            // ── Botão capítulo anterior ──────────────────────────────────────
            Item {
                width: 32; height: 44
                visible: casualCtrl.chapterCount > 0

                Rectangle {
                    id: prevBtnBg
                    anchors.centerIn: parent
                    width: 26; height: 22; radius: 11
                    color: prevChapArea.containsMouse
                           ? Qt.alpha(casualCtrl.accentColor, 0.12)
                           : "transparent"
                    Behavior on color { ColorAnimation { duration: 90 } }

                    Text {
                        anchors.centerIn: parent
                        text: "‹"
                        color: casualCtrl.currentPage <= 0
                               ? Qt.alpha(casualCtrl.mutedColor, 0.20)
                               : prevChapArea.containsMouse
                                 ? casualCtrl.accentColor
                                 : Qt.alpha(casualCtrl.mutedColor, 0.65)
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    MouseArea {
                        id: prevChapArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        enabled:      casualCtrl.currentPage > 0
                        onClicked:    casualCtrl.requestPrevChapter()
                    }
                }
            }

            // ── Percentagem de leitura ───────────────────────────────────────
            Item {
                width: 38; height: 44

                Text {
                    anchors.centerIn: parent
                    text: casualCtrl.hasDocument
                          ? Math.round(casualCtrl.readingProgress * 100) + "%"
                          : "—"
                    color: progressContainer.hovered
                           ? Qt.alpha(casualCtrl.accentColor, 0.85)
                           : Qt.alpha(casualCtrl.mutedColor, 0.70)
                    font {
                        pixelSize:     10
                        family:        "Georgia, serif"
                        letterSpacing: 0.5
                        weight:        progressContainer.hovered ? Font.Medium : Font.Normal
                    }
                    Behavior on color  { ColorAnimation  { duration: 160 } }
                    Behavior on font.weight { }
                }
            }

            // ── Separador ────────────────────────────────────────────────────
            Item { width: 4; height: 1 }

            // ── Barra de progresso ───────────────────────────────────────────
            Item {
                id: progressContainer
                Layout.fillWidth: true
                height: 44

                readonly property bool hovered:  progressArea.containsMouse || progressArea.pressed
                readonly property int  trackH:   hovered ? 6 : 4
                readonly property real fillRatio: casualCtrl.hasDocument
                                                  ? casualCtrl.readingProgress
                                                  : 0.0

                Behavior on trackH { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                // ── Trilho de fundo ──────────────────────────────────────────
                Rectangle {
                    id: track
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter:   parent.verticalCenter
                    width:  parent.width
                    height: progressContainer.trackH
                    radius: height / 2
                    color:  Qt.alpha(casualCtrl.borderColor,
                                     progressContainer.hovered ? 0.90 : 0.60)

                    Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    Behavior on color  { ColorAnimation  { duration: 130 } }
                }

                // ── Preenchimento com gradiente de brilho ────────────────────
                Rectangle {
                    id: fill
                    anchors {
                        left: track.left
                        verticalCenter: track.verticalCenter
                    }
                    width:  Math.max(track.height, track.width * progressContainer.fillRatio)
                    height: track.height
                    radius: height / 2
                    clip: true

                    // Gradiente sutil no fill: accent puro → levemente mais claro
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: Qt.darker(casualCtrl.accentColor, 1.08)
                        }
                        GradientStop {
                            position: 0.6
                            color: casualCtrl.accentColor
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.lighter(casualCtrl.accentColor, 1.15)
                        }
                    }

                    Behavior on width  { NumberAnimation { duration: 340; easing.type: Easing.OutCubic } }
                    Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                    // Brilho interno — linha fina branca no topo
                    Rectangle {
                        anchors { top: parent.top; left: parent.left; right: parent.right }
                        height: Math.max(1, parent.height * 0.35)
                        radius: height / 2
                        color:  Qt.alpha("white", progressContainer.hovered ? 0.22 : 0.12)
                        Behavior on color { ColorAnimation { duration: 160 } }
                    }
                }

                // ── Ticks de capítulo ────────────────────────────────────────
                Repeater {
                    model: casualCtrl.chapterBreakPositions

                    Item {
                        required property real modelData
                        required property int  index

                        visible: modelData > 0.005 && modelData < 0.995

                        x: track.x + track.width * modelData - 1
                        y: track.y
                        width:  2
                        height: track.height

                        Rectangle {
                            anchors.fill: parent
                            radius: 1
                            color: modelData <= progressContainer.fillRatio
                                   ? Qt.alpha(casualCtrl.headerBg, 0.60)
                                   : Qt.alpha(casualCtrl.mutedColor,
                                               progressContainer.hovered ? 0.55 : 0.0)
                            Behavior on color  { ColorAnimation  { duration: 180 } }
                            Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        }

                        // Tooltip de capítulo aprimorado
                        Rectangle {
                            id: chapTooltip
                            visible: tickArea.containsMouse
                            anchors.bottom: parent.top
                            anchors.bottomMargin: 10
                            anchors.horizontalCenter: parent.horizontalCenter
                            width:  chapLabel.implicitWidth + 14
                            height: 20
                            radius: 6
                            color:  Qt.alpha(casualCtrl.textColor, 0.90)

                            Text {
                                id: chapLabel
                                anchors.centerIn: parent
                                text: "Cap. " + (index + 1)
                                color: casualCtrl.headerBg
                                font { pixelSize: 9; family: "Georgia, serif"; letterSpacing: 0.3 }
                            }

                            // Seta indicadora
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.bottom
                                width: 6; height: 4
                                rotation: 45
                                color: Qt.alpha(casualCtrl.textColor, 0.90)
                                anchors.topMargin: -3
                            }
                        }

                        MouseArea {
                            id: tickArea
                            anchors { fill: parent; margins: -10 }
                            hoverEnabled: true
                            cursorShape:  Qt.PointingHandCursor
                            onClicked:    casualCtrl.setCurrentChapterIndex(index)
                        }
                    }
                }

                // ── Handle — aparece com spring no hover ─────────────────────
                Item {
                    id: handle
                    anchors.verticalCenter: track.verticalCenter
                    x: fill.width - width / 2
                    width: 16; height: 16

                    opacity: progressContainer.hovered ? 1.0 : 0.0
                    scale:   progressContainer.hovered ? 1.0 : 0.2

                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                    Behavior on scale   { NumberAnimation { duration: 240; easing.type: Easing.OutBack } }
                    Behavior on x       { NumberAnimation { duration: 340; easing.type: Easing.OutCubic } }

                    // Anel externo de glow (só ao arrastar)
                    Rectangle {
                        anchors.centerIn: parent
                        width:  progressArea.pressed ? 22 : 16
                        height: progressArea.pressed ? 22 : 16
                        radius: width / 2
                        color: "transparent"
                        border.color: Qt.alpha(casualCtrl.accentColor,
                                               progressArea.pressed ? 0.40 : 0.0)
                        border.width: 2
                        Behavior on width        { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }
                        Behavior on height       { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }
                        Behavior on border.color { ColorAnimation  { duration: 140 } }
                    }

                    // Anel médio (sempre visível no hover)
                    Rectangle {
                        anchors.centerIn: parent
                        width: 16; height: 16; radius: 8
                        color: "transparent"
                        border.color: Qt.alpha(casualCtrl.accentColor, 0.38)
                        border.width: 1.5
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                    }

                    // Núcleo
                    Rectangle {
                        anchors.centerIn: parent
                        width:  progressArea.pressed ? 7 : 9
                        height: progressArea.pressed ? 7 : 9
                        radius: width / 2
                        color:  casualCtrl.accentColor
                        Behavior on width  { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
                        Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
                        Behavior on color  { ColorAnimation  { duration: 150 } }
                    }
                }

                // ── Tooltip de posição ao arrastar (ancorado no handle) ───────
                Rectangle {
                    id: scrubTooltip
                    visible:    progressArea.pressed && casualCtrl.hasDocument
                    anchors.bottom: track.top
                    anchors.bottomMargin: 12
                    x: Math.max(0, Math.min(progressContainer.width - width,
                                handle.x + handle.width / 2 - width / 2))
                    width:  scrubLabel.implicitWidth + 18
                    height: 24
                    radius: 7
                    color:  Qt.alpha(casualCtrl.textColor, 0.92)

                    // Sombra leve
                    layer.enabled: true
                    layer.effect: null

                    Text {
                        id: scrubLabel
                        anchors.centerIn: parent
                        text: {
                            if (!casualCtrl.hasDocument || !progressArea.pressed) return ""
                            const r = Math.max(0, Math.min(1, progressArea.mouseX / track.width))
                            const pct = Math.round(r * 100)
                            const page = Math.round(r * (casualCtrl.totalPages - 1))
                            return pct + "% · p." + (page + 1)
                        }
                        color: casualCtrl.headerBg
                        font { pixelSize: 10; family: "Georgia, serif"; letterSpacing: 0.2 }
                    }

                    // Pontinha inferior
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        width: 8; height: 8
                        rotation: 45
                        color: Qt.alpha(casualCtrl.textColor, 0.92)
                    }
                }

                // ── Área de interação ────────────────────────────────────────
                MouseArea {
                    id: progressArea
                    anchors.fill: parent
                    hoverEnabled:  true
                    cursorShape:   progressArea.pressed
                                   ? Qt.SizeHorCursor
                                   : Qt.PointingHandCursor

                    onClicked: function(mouse) {
                        const r = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                        const page = Math.round(r * (casualCtrl.totalPages - 1))
                        casualCtrl.navigateToPage(page)
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed) return
                        const r = Math.max(0.0, Math.min(1.0, mouse.x / track.width))
                        const page = Math.round(r * (casualCtrl.totalPages - 1))
                        casualCtrl.navigateToPage(page)
                    }
                }
            }

            // ── Separador ────────────────────────────────────────────────────
            Item { width: 4; height: 1 }

            // ── Localização (página) ─────────────────────────────────────────
            Item {
                width: 72; height: 44
                visible: casualCtrl.hasDocument

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (!casualCtrl.hasDocument || casualCtrl.totalPages <= 0) return ""
                        return "p. " + (casualCtrl.currentPage * 2 + 1)
                               + " / " + casualCtrl.totalPages
                    }
                    color: progressContainer.hovered
                           ? Qt.alpha(casualCtrl.accentColor, 0.75)
                           : Qt.alpha(casualCtrl.mutedColor, 0.60)
                    font {
                        pixelSize:     10
                        family:        "Georgia, serif"
                        letterSpacing: 0.2
                    }
                    Behavior on color { ColorAnimation { duration: 160 } }
                }
            }

            // ── Botão capítulo seguinte ──────────────────────────────────────
            Item {
                width: 32; height: 44
                visible: casualCtrl.chapterCount > 0

                Rectangle {
                    id: nextBtnBg
                    anchors.centerIn: parent
                    width: 26; height: 22; radius: 11
                    color: nextChapArea.containsMouse
                           ? Qt.alpha(casualCtrl.accentColor, 0.12)
                           : "transparent"
                    Behavior on color { ColorAnimation { duration: 90 } }

                    Text {
                        anchors.centerIn: parent
                        text: "›"
                        color: casualCtrl.currentPage >= casualCtrl.totalPages - 1
                               ? Qt.alpha(casualCtrl.mutedColor, 0.20)
                               : nextChapArea.containsMouse
                                 ? casualCtrl.accentColor
                                 : Qt.alpha(casualCtrl.mutedColor, 0.65)
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    MouseArea {
                        id: nextChapArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  Qt.PointingHandCursor
                        enabled:      casualCtrl.currentPage < casualCtrl.totalPages - 1
                        onClicked:    casualCtrl.requestNextChapter()
                    }
                }
            }
        }
    }
}
