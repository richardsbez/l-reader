// ─────────────────────────────────────────────────────────────────────────────
// src/qml/casual/CasualModeHeader.qml  —  l-reader · Modo Casual
//
// HeaderBar minimalista que se funde com a cor de fundo do tema.
// Layout:  [ ≡ | Título do Capítulo ]  ·····  [ 🔍 | Aa ]
//
// Altura fixa: 44 px (espelha Layout::Toolbar::kSideToggleSize do C++)
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    height: 44
    color:  casualCtrl.headerBg

    Behavior on color {
        ColorAnimation { duration: 220; easing.type: Easing.OutQuad }
    }

    // ── Popover de configurações (instanciado aqui, acedido pelo botão Aa) ───
    TextSettingsPopover {
        id: settingsPopover
        // Ancora o popup abaixo do botão de configurações
        parent: settingsBtn
        x: settingsBtn.width - width      // alinha à direita do botão
        y: settingsBtn.height + 4
    }

    // ── Layout principal — Row com três zonas ─────────────────────────────────
    RowLayout {
        anchors {
            fill:           parent
            leftMargin:     8
            rightMargin:    8
        }
        spacing: 0

        // ────────────────────────────────────────────────────────────────────
        // ZONA ESQUERDA — toggle da sidebar + título do capítulo
        // ────────────────────────────────────────────────────────────────────
        RowLayout {
            spacing: 4
            Layout.alignment: Qt.AlignVCenter

            // Botão de toggle da sidebar
            HeaderIconButton {
                id:        sidebarBtn
                iconText:  "☰"
                toolTip:   "Alternar painel lateral"
                checked:   casualCtrl.sidebarOpen
                onClicked: casualCtrl.toggleSidebar()
            }

            // Separador visual fino
            Rectangle {
                width:  1
                height: 18
                color:  casualCtrl.borderColor
                opacity: 0.6
                Layout.alignment: Qt.AlignVCenter
            }

            // Título do capítulo — truncado com elipsis se necessário
            Text {
                id: chapterLabel
                Layout.maximumWidth: 320
                text:      casualCtrl.chapterTitle
                elide:     Text.ElideRight
                font {
                    family:    "Georgia, serif"
                    pixelSize: 13
                    weight:    Font.Normal
                }
                color:   casualCtrl.mutedColor
                opacity: 0.85

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // ZONA CENTRAL — espaçador flexível
        // ────────────────────────────────────────────────────────────────────
        Item { Layout.fillWidth: true }

        // ────────────────────────────────────────────────────────────────────
        // ZONA DIREITA — pesquisa + configurações de texto
        // ────────────────────────────────────────────────────────────────────
        RowLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter

            // Botão de pesquisa (lupa)
            HeaderIconButton {
                id:        searchBtn
                iconText:  "🔍"
                toolTip:   "Pesquisar no texto"
                checked:   casualCtrl.searchOpen
                onClicked: casualCtrl.toggleSearch()
            }

            // Botão de configurações tipográficas (Aa)
            HeaderIconButton {
                id:        settingsBtn
                // Usa texto "Aa" em vez de ícone para máximo minimalismo
                iconText:  "Aa"
                toolTip:   "Configurações de leitura"
                checked:   settingsPopover.visible
                font.pixelSize: 12
                font.weight:    Font.Medium
                onClicked: {
                    if (settingsPopover.visible)
                        settingsPopover.close()
                    else
                        settingsPopover.open()
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Componente interno — botão de ícone discreto para o header
    // Separado num type inline para reutilização sem ficheiro extra
    // ─────────────────────────────────────────────────────────────────────────────
    component HeaderIconButton: Rectangle {
        id: btn

        // API pública
        property string iconText:  ""
        property string toolTip:   ""
        property bool   checked:   false
        property alias  font:      label.font

        signal clicked()

        width:  34
        height: 30
        radius: 6
        color:  btnArea.containsMouse
                    ? Qt.alpha(casualCtrl.textColor, 0.08)
                    : checked
                        ? Qt.alpha(casualCtrl.accentColor, 0.12)
                        : "transparent"

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        // Label central
        Text {
            id: label
            anchors.centerIn: parent
            text:  btn.iconText
            color: btn.checked ? casualCtrl.accentColor : casualCtrl.mutedColor
            font.pixelSize: 14

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }

        // Tooltip nativo
        ToolTip.visible:  btnArea.containsMouse && btn.toolTip.length > 0
        ToolTip.text:     btn.toolTip
        ToolTip.delay:    700

        MouseArea {
            id:          btnArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape:  Qt.PointingHandCursor
            onClicked:    btn.clicked()
        }
    }
}
