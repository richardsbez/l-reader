// casual_mode.hpp  —  Modo Casual
// Layout: sem toolbar principal, sidebar esquerda com info do livro + TOC flat,
//         barra de navegação inferior, faixa de capítulo no topo.
// Conteúdo com margens generosas para leitura confortável (estilo e-reader).
#pragma once
#include <QDockWidget>
#include "reading_mode.hpp"
#include <QMargins>

class CasualMode final : public ReadingMode {
public:
    void enter(MainWindow& ctx) override {
        ctx.applySS(QStringLiteral(":/styles/casual.qss"));

        // Toolbar principal: OCULTA (substituída pela nav inferior)
        if (ctx.toolbar())       ctx.toolbar()->hide();

        // Painel de anotações: oculto
        if (ctx.annotDock())     ctx.annotDock()->hide();

        // Sidebar: aberta com book info + TOC flat
        ctx.setSidebarCasualMode(true);
        if (ctx.sidebar())       ctx.sidebar()->show();

        // Barra de navegação inferior: OCULTA — substituída pela tab bar da sidebar
        // if (ctx.bottomNav())     ctx.bottomNav()->show();

        // Faixa de capítulo no topo: oculta — título já aparece no header da sidebar
        // if (ctx.chapterHeader()) ctx.chapterHeader()->show();

        // Margens generosas para layout estilo e-reader (duas colunas de texto)
        ctx.setMargins(QMargins(80, 24, 80, 24));
    }

    void exit(MainWindow& ctx) override {
        // Restaura toolbar e esconde elementos exclusivos do Casual
        if (ctx.toolbar())       ctx.toolbar()->setVisible(true);
        if (ctx.bottomNav())     ctx.bottomNav()->hide();
        if (ctx.chapterHeader()) ctx.chapterHeader()->hide();
        if (ctx.sidebar())       ctx.sidebar()->hide();
        ctx.setSidebarCasualMode(false);
    }

    ModeType type() const override { return ModeType::Casual; }
    QString  name() const override { return QStringLiteral("Casual"); }
};
