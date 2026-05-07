// reading_mode.hpp
#pragma once
#include <QString>

class MainWindow;  // forward declaration — evita dependência circular

enum class ModeType { Standard, Study, Casual };

class ReadingMode {
public:
    virtual ~ReadingMode() = default;

    // enter: aplica o estado (carrega QSS, mostra/esconde widgets)
    virtual void enter(MainWindow& ctx) = 0;

    // exit: desfaz exclusivamente o que enter() fez
    virtual void exit(MainWindow& ctx) = 0;

    virtual ModeType    type()  const = 0;
    virtual QString     name()  const = 0;
};
