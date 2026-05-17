// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/CasualModeWidget.h  —  l-reader · Modo Casual
//
// Widget de integração: hospeda o motor QML (CasualModeView) dentro da
// arquitectura QWidget existente usando QQuickWidget.
//
// Responsabilidades:
//   • Criar e gerir CasualModeController (ciclo de vida ligado ao widget)
//   • Expor o controller ao QML via setContextProperty("casualCtrl", ...)
//   • Aplicar o estilo Quick Controls (Basic — sem dependência de Material)
//   • Fornecer a URL do QML raiz compilada via qt_add_qml_module()
//
// Utilização em MainWindow:
//   auto* casualWidget = new CasualModeWidget(this);
//   centralStack->addWidget(casualWidget);   // QStackedWidget
//   // Para activar:
//   centralStack->setCurrentWidget(casualWidget);
//   casualWidget->controller()->setSidebarOpen(false);
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QQuickWidget>
#include <QUrl>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>

#include "CasualModeController.h"

class CasualModeWidget final : public QQuickWidget
{
    Q_OBJECT

public:
    explicit CasualModeWidget(QWidget* parent = nullptr)
        : QQuickWidget(parent)
        , m_controller(new CasualModeController(this))
    {
        // ── 1. Estilo dos Quick Controls ─────────────────────────────────────
        // "Basic" é o único estilo sem dependência de plataforma (não usa
        // Material/Fusion/Windows específicos) — ideal para uma UI custom.
        QQuickStyle::setStyle(QStringLiteral("Basic"));

        // ── 2. Expor o controller ao motor QML ───────────────────────────────
        // O ficheiro CasualModeView.qml acede ao controller como "casualCtrl".
        rootContext()->setContextProperty(
            QStringLiteral("casualCtrl"),
            m_controller
        );

        // ── 3. Comportamento do widget ────────────────────────────────────────
        setResizeMode(QQuickWidget::SizeRootObjectToView);
        setAttribute(Qt::WA_OpaquePaintEvent);       // evita flicker no resize
        setAttribute(Qt::WA_NoSystemBackground);

        // ── 4. Carregar o QML raiz ────────────────────────────────────────────
        // O caminho resulta de:
        //   RESOURCE_PREFIX "/LReader"  +  URI "LReader.Casual"  → pasta "LReader/Casual"
        //   + nome do ficheiro QML
        setSource(QUrl(QStringLiteral(
            "qrc:/LReader/LReader/Casual/CasualModeView.qml"
        )));
    }

    // Acesso ao controller para configuração externa (ex: ModeManager)
    [[nodiscard]] CasualModeController* controller() const noexcept
    {
        return m_controller;
    }

private:
    CasualModeController* const m_controller;
};
