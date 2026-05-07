// ─────────────────────────────────────────────────────────────────────────────
// src/Ui/layout_tokens.hpp  —  l-reader
//
// Tokens de layout: TODOS os tamanhos, margens e espaçamentos da UI.
// Nenhuma dimensão deve ser escrita literalmente em main_window.cpp.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QSize>
#include <QMargins>

namespace Layout {

// ── Toolbar principal ────────────────────────────────────────────────────────
namespace Toolbar {
    inline constexpr QSize kIconSize        { 16, 16 };
    inline constexpr QSize kSideToggleSize  { 34, 30 };
    inline constexpr QSize kZoomBtnSize     { 26, 30 };
    inline constexpr int   kZoomLabelWidth  { 46 };
    inline constexpr QSize kNavBtnSize      { 28, 30 };
    inline constexpr int   kPageLabelWidth  { 80 };
    inline constexpr QSize kToolBtnSize     { 32, 30 };
    inline constexpr QSize kHamburgerSize   { 32, 30 };
    inline constexpr QSize kModeDropdownSize{ 96, 30 };  // largura fixa do dropdown de modos

} // namespace Toolbar

// ── Barra de ícones do sidebar ───────────────────────────────────────────────
namespace SidebarIconBar {
    inline constexpr QMargins kMargins { 10, 8, 10, 8 };
    inline constexpr int      kSpacing { 6 };
    inline constexpr QSize    kBtnSize { 36, 36 };
} // namespace SidebarIconBar

// ── Painel de anotações — Modo Estudo (dock lado direito) ────────────────────
namespace AnnotPanel {
    inline constexpr int   kWidth   { 72 };
    inline constexpr QSize kBtnSize { 44, 44 };
    inline constexpr QMargins kMargins { 8, 12, 8, 12 };
    inline constexpr int   kSpacing { 8 };
} // namespace AnnotPanel

// ── Barra de navegação inferior — Modo Casual ────────────────────────────────
namespace BottomNav {
    inline constexpr int   kHeight  { 44 };
    inline constexpr QSize kNavBtn  { 36, 36 };
    inline constexpr int   kPadding { 16 };
} // namespace BottomNav

// ── Widget de info do livro — Casual sidebar ─────────────────────────────────
namespace BookInfo {
    inline constexpr QSize kCoverSize { 80, 80 };
    inline constexpr int   kHeight    { 148 };
} // namespace BookInfo

// ── Árvore de sumário ─────────────────────────────────────────────────────────
namespace TocTree {
    inline constexpr int kIndentation { 14 };
} // namespace TocTree

// ── Status bar ───────────────────────────────────────────────────────────────
namespace StatusBar {
    inline constexpr QMargins kFileLabelMargins { 4, 0, 0, 0 };
    inline constexpr QMargins kModeLabelMargins { 0, 0, 4, 0 };
} // namespace StatusBar

// ── Estrutural ───────────────────────────────────────────────────────────────
namespace Structural {
    inline constexpr QMargins kNoMargins { 0, 0, 0, 0 };
    inline constexpr int      kNoSpacing { 0 };
} // namespace Structural

} // namespace Layout
