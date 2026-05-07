// mode_manager.cpp
#include "mode_manager.hpp"
#include "ViewLayer/main_window.hpp"   // necessário antes dos mode headers (tipo completo)
#include "standard_mode.hpp"
#include "study_mode.hpp"
#include "casual_mode.hpp"
#include <stdexcept>

ModeManager::ModeManager(MainWindow* window, QObject* parent)
    : QObject(parent), m_window(window)
{
    // Regra: todo arquivo novo abre em Standard
    m_current = makeMode(ModeType::Standard);
    m_current->enter(*m_window);
}

void ModeManager::transitionTo(ModeType target)
{
    if (m_current && m_current->type() == target)
        return;  // no-op — evita reentrada

    const ModeType previous = m_current ? m_current->type() : ModeType::Standard;

    if (m_current)
        m_current->exit(*m_window);

    m_current = makeMode(target);
    m_current->enter(*m_window);

    emit modeChanged(target, previous);
}

ModeType ModeManager::currentMode() const
{
    return m_current ? m_current->type() : ModeType::Standard;
}

// Implementação ausente no original — necessária para a status bar.
QString ModeManager::currentName() const
{
    return m_current ? m_current->name() : QStringLiteral("Padrão");
}

std::unique_ptr<ReadingMode> ModeManager::makeMode(ModeType type)
{
    switch (type) {
        case ModeType::Standard: return std::make_unique<StandardMode>();
        case ModeType::Study:    return std::make_unique<StudyMode>();
        case ModeType::Casual:   return std::make_unique<CasualMode>();
    }
    throw std::invalid_argument("ModeType desconhecido");
}
