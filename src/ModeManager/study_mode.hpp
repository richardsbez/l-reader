// study_mode.hpp  —  Modo Estudo
// Layout: toolbar top, sidebar esquerda aberta (ícones + TOC),
//         painel de anotações à direita, sem nav inferior.
#pragma once
#include <QDockWidget>
#include "reading_mode.hpp"
#include <QMargins>

class StudyMode final : public ReadingMode {
public:
    void enter(MainWindow& ctx) override {
        ctx.applySS(QStringLiteral(":/styles/study.qss"));

        // Toolbar principal: visível
        if (ctx.toolbar())       ctx.toolbar()->setVisible(true);

        // Sidebar esquerda: aberta com ícones + TOC
        ctx.setSidebarCasualMode(false);
        if (ctx.sidebar())       ctx.sidebar()->show();

        // Painel de anotações à direita: visível
        if (ctx.annotDock())     ctx.annotDock()->show();

        // Elementos do Casual: ocultos
        if (ctx.bottomNav())     ctx.bottomNav()->hide();
        if (ctx.chapterHeader()) ctx.chapterHeader()->hide();

        // Margens reduzidas para maximizar área de leitura
        ctx.setMargins(QMargins(8, 8, 8, 8));
    }

    void exit(MainWindow& ctx) override {
        if (ctx.sidebar())   ctx.sidebar()->hide();
        if (ctx.annotDock()) ctx.annotDock()->hide();
    }

    ModeType type() const override { return ModeType::Study; }
    QString  name() const override { return QStringLiteral("Estudo"); }
};
