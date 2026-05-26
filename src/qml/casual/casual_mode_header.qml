// ─────────────────────────────────────────────────────────────────────────────
// casual_mode_header.qml  —  l-reader · Modo Casual
//
// Espelha o layout da Image 2 (Apple Books / Kindle desktop):
//
//   ┌────────────────────────┬────────────────────────────────────────┐
//   │   Heróis & Vilões      │           CAPÍTULO 1                   │
//   └────────────────────────┴────────────────────────────────────────┘
//
// Título do livro alinhado ao centro da página esquerda.
// Título do capítulo alinhado ao centro da página direita.
// Altura: 32px (discreto, editorial).
// ─────────────────────────────────────────────────────────────────────────────
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    height: 32
    color:  casualCtrl.headerBg

    Behavior on color {
        ColorAnimation { duration: 220; easing.type: Easing.OutQuad }
    }

    // Popover de configurações (acessado pelo botão Aa no canto direito)
    TextSettingsPopover {
        id: settingsPopover
        parent: settingsBtn
        x: settingsBtn.width - width
        y: settingsBtn.height + 4
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Metade esquerda: título do livro ──────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                anchors.centerIn: parent
                text:  casualCtrl.bookTitle
                color: casualCtrl.mutedColor
                font { pixelSize: 11; family: "Georgia, serif"; italic: true }
                elide: Text.ElideRight
                maximumLineCount: 1
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter

                Behavior on color { ColorAnimation { duration: 200 } }
            }
        }

        // ── Divisor central (alinhado com o gutter das páginas) ───────────
        Rectangle {
            width:  1
            height: 16
            color:  casualCtrl.borderColor
            opacity: 0.4
            Layout.alignment: Qt.AlignVCenter
        }

        // ── Metade direita: título do capítulo + botão Aa ─────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Título do capítulo centralizado
            Text {
                anchors.centerIn: parent
                text:  casualCtrl.chapterTitle.toUpperCase()
                color: casualCtrl.mutedColor
                font { pixelSize: 11; family: "Georgia, serif"; letterSpacing: 0.8 }
                elide: Text.ElideRight
                maximumLineCount: 1
                width: parent.width - 48
                horizontalAlignment: Text.AlignHCenter

                Behavior on color { ColorAnimation { duration: 200 } }
            }

            // Botão Aa — fixado à direita
            HeaderIconButton {
                id: settingsBtn
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                iconText:  "Aa"
                toolTip:   "Configurações de texto"
                font.pixelSize: 11
                onClicked: settingsPopover.visible = !settingsPopover.visible
            }
        }
    }

    // ── Componente local: botão de ícone ──────────────────────────────────
    component HeaderIconButton: Rectangle {
        id: iconBtn
        property string iconText: ""
        property string toolTip:  ""
        property bool   checked:  false

        signal clicked()

        width:  28; height: 24
        radius: 6
        color: btnArea.containsMouse
                   ? Qt.alpha(casualCtrl.textColor, checked ? 0.12 : 0.07)
                   : checked
                       ? Qt.alpha(casualCtrl.accentColor, 0.14)
                       : "transparent"

        Behavior on color { ColorAnimation { duration: 100 } }

        Text {
            anchors.centerIn: parent
            text:  iconBtn.iconText
            color: iconBtn.checked ? casualCtrl.accentColor : casualCtrl.mutedColor
            font:  iconBtn.font
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        ToolTip.visible: btnArea.containsMouse && iconBtn.toolTip.length > 0
        ToolTip.text:    iconBtn.toolTip
        ToolTip.delay:   600

        MouseArea {
            id: btnArea
            anchors.fill: parent
            hoverEnabled:  true
            cursorShape:   Qt.PointingHandCursor
            onClicked:     iconBtn.clicked()
        }
    }
}
