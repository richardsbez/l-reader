// casual_mode.hpp  —  Modo Casual
// Layout: sem toolbar principal, sidebar esquerda com info do livro + TOC flat,
//         barra de navegação inferior, faixa de capítulo no topo.
// Conteúdo com margens generosas para leitura confortável (estilo e-reader).
#pragma once
#include "reading_mode.hpp"
#include <QDockWidget>
#include <QMainWindow>
#include <QMargins>
#include <QStatusBar>

class CasualMode final : public ReadingMode {
public:
  void enter(MainWindow &ctx) override {
    ctx.applySS(QStringLiteral(":/styles/casual.qss"));

    // Barras principal e inferior ficam ocultas por defeito no Modo Casual.
    // O mecanismo auto-hide (eventFilter) as mostra quando o rato se
    // aproxima da borda superior (toolbar) ou inferior (bottomNav).
    if (ctx.toolbar())
      ctx.toolbar()->hide();
    if (ctx.bottomNav())
      ctx.bottomNav()->hide();

    // Status bar oculta no Modo Casual: a sidebar deve descer até ao fundo
    // da janela sem o espaço reservado pela status bar.
    ctx.statusBar()->hide();

    // Painel de anotações: oculto
    if (ctx.annotDock())
      ctx.annotDock()->hide();

    // Sidebar: aberta com book info + TOC flat
    ctx.setSidebarCasualMode(true);
    if (ctx.sidebar())
      ctx.sidebar()->show();

    // Ativa auto-hide: instala filtro global de rato
    ctx.enableCasualAutoHide(true);

    // Margens generosas para layout estilo e-reader (duas colunas de texto)
    ctx.setMargins(QMargins(80, 24, 80, 24));
  }

  void exit(MainWindow &ctx) override {
    // Desativa auto-hide antes de restaurar a visibilidade normal
    ctx.enableCasualAutoHide(false);

    // Restaura toolbar, status bar e esconde elementos exclusivos do Casual
    if (ctx.toolbar())
      ctx.toolbar()->setVisible(true);
    ctx.statusBar()->show();
    if (ctx.bottomNav())
      ctx.bottomNav()->hide();
    if (ctx.chapterHeader())
      ctx.chapterHeader()->hide();
    if (ctx.sidebar())
      ctx.sidebar()->hide();
    ctx.setSidebarCasualMode(false);
  }

  ModeType type() const override { return ModeType::Casual; }
  QString name() const override { return QStringLiteral("Casual"); }
};
