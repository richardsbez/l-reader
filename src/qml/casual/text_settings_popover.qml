// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/text_settings_popover.qml  —  l-reader · Modo Casual
//
// Popover flutuante de configurações de leitura.
// Surge ao clicar no botão "Aa" da HeaderBar.
//
// Secções:
//   1. Tamanho de fonte  (A−  ·  slider visual  ·  A+)
//   2. Margens           (◁   ·  indicador      ·  ▷)
//   3. Espaçamento de linha
//   4. Tema de cor       (Claro | Escuro | Sépia | Solarized)
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: root

    width:  280
    height: contentCol.implicitHeight + 32

    // ── Aparência da caixa flutuante ──────────────────────────────────────────
    background: Rectangle {
        color:        casualCtrl.headerBg
        border.color: casualCtrl.borderColor
        border.width: 1
        radius:       12

        // Sombra suave (simulada com um segundo rect menor)
        layer.enabled: true
        layer.effect: null   // efeito real requer Qt.labs.platform — mantemos simples

        Rectangle {
            anchors {
                fill:        parent
                topMargin:   2
                leftMargin:  2
                rightMargin: -2
                bottomMargin: -2
            }
            color:   Qt.alpha("#000000", 0.06)
            radius:  parent.radius + 2
            z:       -1
        }
    }

    // Animação de entrada/saída
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 160; easing.type: Easing.OutQuad }
        NumberAnimation { property: "scale";   from: 0.93; to: 1.0; duration: 160; easing.type: Easing.OutQuad }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 120; easing.type: Easing.InQuad }
    }

    // Fecha ao clicar fora
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    // ── Conteúdo ──────────────────────────────────────────────────────────────
    ColumnLayout {
        id: contentCol
        anchors {
            top:         parent.top
            left:        parent.left
            right:       parent.right
            topMargin:   16
            leftMargin:  16
            rightMargin: 16
        }
        spacing: 0

        // ── Secção: Tamanho de Fonte ──────────────────────────────────────────
        SectionLabel { text: "Tamanho do Texto" }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing:          8

            // Botão A− (reduzir fonte)
            FontSizeButton {
                text:      "A−"
                font.pixelSize: 13
                onClicked: casualCtrl.decreaseFontSize()
                enabled:   casualCtrl.fontSize > 10
            }

            // Barra de pré-visualização do tamanho actual
            Item {
                Layout.fillWidth: true
                height: 32

                // Trilho
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 3
                    radius: 2
                    color: casualCtrl.borderColor
                }

                // Preenchimento proporcional
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    width: parent.width * ((casualCtrl.fontSize - 10) / 22.0)
                    height: 3
                    radius: 2
                    color: casualCtrl.accentColor

                    Behavior on width {
                        NumberAnimation { duration: 100 }
                    }
                }

                // Indicador numérico central
                Text {
                    anchors.centerIn: parent
                    text:  casualCtrl.fontSize + " px"
                    color: casualCtrl.mutedColor
                    font.pixelSize: 11
                }
            }

            // Botão A+ (aumentar fonte)
            FontSizeButton {
                text:      "A+"
                font.pixelSize: 17
                onClicked: casualCtrl.increaseFontSize()
                enabled:   casualCtrl.fontSize < 32
            }
        }

        // ── Divisor ───────────────────────────────────────────────────────────
        PopoverDivider { Layout.topMargin: 14 }

        // ── Secção: Margens ───────────────────────────────────────────────────
        SectionLabel { text: "Margens"; Layout.topMargin: 12 }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8

            FontSizeButton {
                text: "◁"
                onClicked: casualCtrl.decreaseMargin()
                enabled: casualCtrl.columnMargin > 24
            }

            // Visualização das margens como preview de página
            Item {
                Layout.fillWidth: true
                height: 32

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: casualCtrl.borderColor
                    border.width: 1
                    radius: 3

                    // Área de texto mockada (margem proporcional)
                    Rectangle {
                        property real ratio: (casualCtrl.columnMargin - 24) / (160 - 24)
                        anchors {
                            fill:          parent
                            leftMargin:    4 + ratio * 16
                            rightMargin:   4 + ratio * 16
                            topMargin:     4
                            bottomMargin:  4
                        }
                        color:  Qt.alpha(casualCtrl.textColor, 0.08)
                        radius: 2

                        Behavior on anchors.leftMargin {
                            NumberAnimation { duration: 100 }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text:  casualCtrl.columnMargin + " px"
                        color: casualCtrl.mutedColor
                        font.pixelSize: 10
                    }
                }
            }

            FontSizeButton {
                text: "▷"
                onClicked: casualCtrl.increaseMargin()
                enabled: casualCtrl.columnMargin < 160
            }
        }

        // ── Divisor ───────────────────────────────────────────────────────────
        PopoverDivider { Layout.topMargin: 14 }

        // ── Secção: Tema de Cor ───────────────────────────────────────────────
        SectionLabel { text: "Tema de Cor"; Layout.topMargin: 12 }

        // Grid 2×2 dos quatro temas
        GridLayout {
            Layout.fillWidth:  true
            Layout.topMargin:  8
            Layout.bottomMargin: 4
            columns:     2
            rowSpacing:  8
            columnSpacing: 8

            // ── Claro ─────────────────────────────────────────────────────────
            ThemeButton {
                bgSwatch:   "#FAFAFA"
                textSwatch: "#1A1A1A"
                label:      "Claro"
                themeIndex: 0
            }

            // ── Escuro ────────────────────────────────────────────────────────
            ThemeButton {
                bgSwatch:   "#1E1E1E"
                textSwatch: "#E8E8E8"
                label:      "Escuro"
                themeIndex: 1
            }

            // ── Sépia ─────────────────────────────────────────────────────────
            ThemeButton {
                bgSwatch:   "#F8F0E3"
                textSwatch: "#3B2D1F"
                label:      "Sépia"
                themeIndex: 2
            }

            // ── Solarized (Creme) ─────────────────────────────────────────────
            ThemeButton {
                bgSwatch:   "#FDF6E3"
                textSwatch: "#657B83"
                label:      "Solarized"
                themeIndex: 3
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Componentes internos inline
    // ─────────────────────────────────────────────────────────────────────────────

    // Label de secção
    component SectionLabel: Text {
        Layout.fillWidth: true
        color:  casualCtrl.mutedColor
        font {
            pixelSize:      10
            weight:         Font.Medium
            letterSpacing:  0.8
        }
        text: ""
    }

    // Divisor horizontal
    component PopoverDivider: Rectangle {
        Layout.fillWidth: true
        height: 1
        color:  casualCtrl.borderColor
        opacity: 0.7
    }

    // Botão de ajuste de tamanho (A+/A−)
    component FontSizeButton: Rectangle {
        id: fsBtn
        property alias text:    fsLabel.text
        property alias font:    fsLabel.font
        property bool  enabled: true
        signal clicked()

        width:  36
        height: 32
        radius: 7
        color:  fsBtnArea.containsMouse && enabled
                    ? Qt.alpha(casualCtrl.textColor, 0.08)
                    : "transparent"
        border.color: casualCtrl.borderColor
        border.width: 1
        opacity: enabled ? 1.0 : 0.38

        Behavior on color {
            ColorAnimation { duration: 100 }
        }

        Text {
            id: fsLabel
            anchors.centerIn: parent
            color: casualCtrl.textColor
            font.pixelSize: 14
            font.family: "Georgia, serif"
        }

        MouseArea {
            id:          fsBtnArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  fsBtn.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked:    if (fsBtn.enabled) fsBtn.clicked()
        }
    }

    // Botão de tema com amostra de cor
    component ThemeButton: Rectangle {
        id: themeBtn
        property string bgSwatch:   "#FFFFFF"
        property string textSwatch: "#000000"
        property string label:      ""
        property int    themeIndex: 0

        Layout.fillWidth: true
        height: 40
        radius: 8

        // Destaque quando é o tema activo
        readonly property bool isActive: casualCtrl.theme === themeIndex
        color:  isActive
                    ? Qt.alpha(casualCtrl.accentColor, 0.12)
                    : (themeArea.containsMouse ? Qt.alpha(casualCtrl.textColor, 0.05) : "transparent")
        border.color: isActive ? casualCtrl.accentColor : casualCtrl.borderColor
        border.width: isActive ? 1.5 : 1

        Behavior on color  { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        RowLayout {
            anchors {
                fill:        parent
                leftMargin:  10
                rightMargin: 10
            }
            spacing: 8

            // Amostra circular da cor de fundo do tema
            Rectangle {
                width:  20
                height: 20
                radius: 10
                color:  themeBtn.bgSwatch
                border.color: casualCtrl.borderColor
                border.width: 1

                // Metade direita mostra a cor do texto
                Rectangle {
                    anchors {
                        right:        parent.right
                        top:          parent.top
                        bottom:       parent.bottom
                    }
                    width:  parent.width / 2
                    color:  themeBtn.textSwatch
                    radius: parent.radius
                    // Mascarar lado esquerdo do rect direito
                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                        width: parent.width / 2
                        color: themeBtn.textSwatch
                    }
                }
            }

            // Nome do tema
            Text {
                Layout.fillWidth: true
                text:       themeBtn.label
                color:      themeBtn.isActive ? casualCtrl.accentColor : casualCtrl.textColor
                font.pixelSize: 12
                font.weight:    themeBtn.isActive ? Font.Medium : Font.Normal

                Behavior on color { ColorAnimation { duration: 150 } }
            }

            // Check mark se activo
            Text {
                visible: themeBtn.isActive
                text:    "✓"
                color:   casualCtrl.accentColor
                font.pixelSize: 12
            }
        }

        MouseArea {
            id:          themeArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    casualCtrl.setTheme(themeBtn.themeIndex)
        }
    }
}
