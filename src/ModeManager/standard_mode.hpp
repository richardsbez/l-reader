// standard_mode.hpp  —  Modo Padrão
// Layout: toolbar top visível, sidebar fechada, sem painel de anotações,
//         sem barra de navegação inferior.
#pragma once
#include <QDockWidget>
#include "reading_mode.hpp"
#include <QMargins>

class StandardMode final : public ReadingMode {
public:
    void enter(MainWindow& ctx) override {
        ctx.applySS(QStringLiteral(":/styles/standard.qss"));

        // Toolbar principal: visível
        if (ctx.toolbar())       ctx.toolbar()->setVisible(true);

        // Sidebar: fechada (utilizador abre com toggle)
        if (ctx.sidebar())       ctx.sidebar()->hide();

        // Painel de anotações (Estudo): oculto
        if (ctx.annotDock())     ctx.annotDock()->hide();

        // Barra inferior e cabeçalho de capítulo (Casual): ocultos
        if (ctx.bottomNav())     ctx.bottomNav()->hide();
        if (ctx.chapterHeader()) ctx.chapterHeader()->hide();

        // Sidebar em modo padrão (ícones + TOC tree)
        ctx.setSidebarCasualMode(false);

        // Margens do conteúdo
        ctx.setMargins(QMargins(16, 16, 16, 16));
    }

    void exit(MainWindow& ctx) override { Q_UNUSED(ctx); }

    ModeType type() const override { return ModeType::Standard; }
    QString  name() const override { return QStringLiteral("Padrão"); }
};
