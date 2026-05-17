// highlight_entry.hpp
// ─────────────────────────────────────────────────────────────────────────────
// Tipos de dados compartilhados entre PdfCanvasView (renderização)
// e HighlightManager (persistência).  Não depende de Qt nem de Poppler além
// dos tipos básicos — pode ser incluído em qualquer unidade.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>

// ─── Trecho de um highlight em uma única página ───────────────────────────────
struct HighlightPageSpan {
    int             page    = -1;
    QVector<QRectF> ptRects;         ///< rects por caractere/palavra (coords PDF, 72 dpi)
    QVector<QRectF> mergedPtRects;   ///< cache: mergeRectsByLine(ptRects) — preenchido
                                     ///< por PdfCanvasView::addHighlight(); vazio quando
                                     ///< carregado do disco (recalculado no primeiro uso).
};

// ─── Highlight completo (pode abranger múltiplas páginas) ─────────────────────
struct HighlightEntry {
    QString                    id;
    QString                    text;
    QVector<HighlightPageSpan> spans;
    QColor                     color { 255, 220, 0, 110 };

    [[nodiscard]] int firstPage() const {
        return spans.isEmpty() ? -1 : spans.first().page;
    }
    [[nodiscard]] bool isValid() const {
        return !id.isEmpty() && !spans.isEmpty();
    }
};
