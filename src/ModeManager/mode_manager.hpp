// mode_manager.hpp
#pragma once
#include <QObject>
#include <memory>
#include "reading_mode.hpp"

class ModeManager final : public QObject {
    Q_OBJECT
public:
    explicit ModeManager(MainWindow* window, QObject* parent = nullptr);

    // Única entrada pública. Toda troca de modo passa por aqui.
    // Regra: transição APENAS manual (UI ou CommandPalette) — nunca automática.
    void transitionTo(ModeType target);

    [[nodiscard]] ModeType  currentMode() const;
    [[nodiscard]] QString   currentName() const;

signals:
    void modeChanged(ModeType newMode, ModeType previousMode);

private:
    std::unique_ptr<ReadingMode> makeMode(ModeType type);

    MainWindow*                  m_window;
    std::unique_ptr<ReadingMode> m_current;
};
