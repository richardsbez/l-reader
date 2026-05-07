// ─────────────────────────────────────────────────────────────────────────────
// src/Ui/epub_style.hpp  —  l-reader
//
// CSS injectado no QWebEngineView após cada capítulo EPUB.
//
// Separado em ficheiro próprio porque é uma decisão puramente de apresentação:
// tipografia, espaçamento, largura de coluna e cor do texto de leitura.
// main_window.cpp não deve conter regras de estilo em raw strings.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

namespace EpubStyle {

/// CSS normalizado de leitura injectado via JavaScript no QWebEngineView.
///
/// Objectivos:
///   1. Remove o layout em colunas que causava o conteúdo cortado a meio.
///   2. Centra o conteúdo numa coluna de leitura confortável (720 px).
///   3. Aplica tipografia serifada com entrelinha generosa.
///
/// Para alterar a aparência do leitor EPUB edite apenas este ficheiro —
/// não toque em main_window.cpp.
inline constexpr const char* kReadingCSS = R"CSS(
html, body {
    column-width:  unset !important;
    column-count:  unset !important;
    column-fill:   unset !important;
    height:        auto  !important;
    overflow:      visible !important;
    max-height:    none  !important;
}
body {
    max-width:   720px    !important;
    margin:      0 auto   !important;
    padding:     2em 1.5em !important;
    font-family: Georgia, "Palatino Linotype", serif !important;
    font-size:   1.1rem   !important;
    line-height: 1.75     !important;
    color:       #1a1a1a  !important;
}
img  { max-width: 100% !important; height: auto !important; }
pre, code { font-size: 0.9em !important; overflow-x: auto !important; }
)CSS";

} // namespace EpubStyle
