// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/casual_mode_widget.hpp  —  l-reader · Modo Casual
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

#include "casual_mode_controller.hpp"

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
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);

        // ── 4. QML carregado no primeiro showEvent ────────────────────────────
        // Adiado para evitar crash ao iniciar se o widget nunca for exibido
        // (ex.: usuário abre PDF e nunca ativa o modo Casual EPUB).
    }

    [[nodiscard]] CasualModeController* controller() const noexcept
    {
        return m_controller;
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        if (!m_qmlLoaded) {
            m_qmlLoaded = true;
            setSource(QUrl(QStringLiteral(
                "qrc:/LReader/LReader/Casual/CasualModeView.qml"
            )));
        }
        QQuickWidget::showEvent(event);
    }

private:
    CasualModeController* const m_controller;
    bool                        m_qmlLoaded = false;
};
