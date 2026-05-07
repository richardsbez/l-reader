// ─────────────────────────────────────────────────────────────────────────────
// src/Ui/ui_strings.hpp  —  l-reader
//
// Texto de apresentação da UI: TODOS os símbolos, rótulos e placeholders.
// Nenhum literal de texto visual deve existir em main_window.cpp.
//
// REGRA DE ENCODING:
//   • QLatin1StringView → somente caracteres ASCII (0x00–0x7F)
//   • const char*       → UTF-8; use sempre QString::fromUtf8() na chamada
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QLatin1StringView>

namespace UiStr {

// ── Toolbar — grupo esquerdo ─────────────────────────────────────────────────
inline constexpr const char* kSidebarToggle = ":/icons/Sidebar.svg";
inline constexpr const char* kZoomOut       = ":/icons/Minus.svg";
inline constexpr QLatin1StringView kZoomDefault { "100%" };
inline constexpr const char* kZoomIn        = ":/icons/Plus.svg";

inline constexpr const char* kViewModeIcon      = ":/icons/ViewMode.svg";
inline constexpr QLatin1StringView kViewModeText { "View Mode" };

// ── Toolbar — grupo central ───────────────────────────────────────────────────
inline constexpr const char* kPrevPage  = ":/icons/Down.svg";
inline constexpr QLatin1StringView kPageDefault { "\xe2\x80\x94" };  // — (UTF-8)
inline constexpr const char* kNextPage  = ":/icons/Up.svg";

// ── Toolbar — grupo direito ───────────────────────────────────────────────────
struct UiToolDef {
    const char* iconPath;
    const char* objectName;
    const char* text;
};

inline constexpr UiToolDef kUiTools[] = {
    { ":/icons/Move.svg",       "toolMoveBtn",     "Mover"     },
    { ":/icons/Navigation.svg", "toolSelectBtn",   "Selecionar"},
    { ":/icons/EditYellow.svg", "toolAnnotateBtn", "Anotar"    },
};

// ── Toolbar — secção de modos ─────────────────────────────────────────────────
inline constexpr QLatin1StringView kModesLabel       { "Modo" };
inline constexpr const char*       kModeStd          = "Padrão";
inline constexpr const char*       kModeDropdownArrow = "  v";
inline constexpr const char*       kHamburger         = "=";

// ── Sidebar — Modo Padrão / Estudo ───────────────────────────────────────────
inline constexpr QLatin1StringView kSidebarDockTitle { "" };
inline constexpr QLatin1StringView kSidebarTocLabel  { "" };

struct SideIconDef {
    const char* iconPath;
    const char* objectName;
};

inline constexpr SideIconDef kSideIcons[] = {
    { ":/icons/Search.svg",   "sideBtnSearch"   },
    { ":/icons/Image.svg",    "sideBtnGallery"  },
    { ":/icons/Edit.svg",     "sideBtnEdit"     },
    { ":/icons/Bookmark.svg", "sideBtnBookmark" },
    { ":/icons/List.svg",     "sideBtnList"     },
};

// ── Painel de anotações — Modo Estudo ────────────────────────────────────────
struct AnnotToolDef { const char* text; const char* objectName; };
inline constexpr AnnotToolDef kAnnotTools[] = {
    { ":/icons/NotaSquare.svg","annotBtnNotes"     },   // bloco de notas / sticky note
    { ":/icons/Edit2.svg",     "annotBtnHighlight" },   // edit2 / highlight
    { ":/icons/Edit3.svg",     "annotBtnPen"       },   // edit3 / highlight
    { ":/icons/TypeText.svg",  "annotBtnText"      },   // inserção de texto
    { ":/icons/Underline.svg", "annotBtnUnderline" },   // sublinhado
};
inline constexpr AnnotToolDef kAnnotSettingsTool = { ":/icons/Sliders.svg", "annotBtnSettings" };

inline constexpr QLatin1StringView kAnnotCanvasPlaceholder { "Quadro livre para anotacoes infinitas" };

// ── Casual — barra de navegação inferior ─────────────────────────────────────
inline constexpr const char* kCasualPrev = "<";
inline constexpr const char* kCasualNext = ">";

// ── Casual — sidebar info do livro ───────────────────────────────────────────
inline constexpr QLatin1StringView kBookCoverPlaceholder  { "CAPA\nDO\nLIVRO" };
inline constexpr QLatin1StringView kBookTitlePlaceholder  { "Nome do Livro" };
inline constexpr QLatin1StringView kBookAuthorPlaceholder { "Autor" };

// ── Casual — mini toolbar ─────────────────────────────────────────────────────
struct CasualMiniBtnDef { const char* text; const char* objectName; };
inline constexpr CasualMiniBtnDef kCasualMiniTools[] = {
    { "\xe2\x98\xb0",  "casualBtnToc"      },  // ☰
    { "\xe2\x9c\x8f",  "casualBtnAnnotate" },  // ✏
    { "\xf0\x9f\x94\x96", "casualBtnBookmark" }, // 🔖
};

// ── Painel de Marcadores ──────────────────────────────────────────────────────
// NOTA: strings com emoji/unicode usam const char* (UTF-8).
//       Use sempre QString::fromUtf8(UiStr::kXxx) ao converter.
inline constexpr const char* kBookmarkPanelTitle    = "Marcadores";
inline constexpr const char* kBookmarkAddBtnText    = " Add marcador";
inline constexpr const char* kBookmarkRemoveBtnText = " Remover marcador";
inline constexpr const char* kBookmarkAddBtnObjName = "addBookmarkBtn";
inline constexpr const char* kBookmarkEmptyMsg      = "Nenhum marcador.\nClique em + para adicionar.";
inline constexpr const char* kBookmarkSaveError     = "Erro ao salvar marcador.";
inline constexpr const char* kBookmarkRemoveTooltip = "Remover marcador";
inline constexpr const char* kBookmarkEditPlaceholder = "Nome do marcador...";
inline constexpr const char* kBookmarkEmbedBtnText  = "Embutir no PDF";

} // namespace UiStr
