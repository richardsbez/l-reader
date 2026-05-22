cmake_minimum_required(VERSION 3.25)

project(l-reader
    VERSION     0.1.0
    DESCRIPTION "Leitor de documentos multiplataforma (PDF / EPUB / MOBI / Markdown)"
    LANGUAGES   CXX
)

# ─────────────────────────────────────────────────────────────────────────────
# Padrão de linguagem — C++20 estrito, sem extensões de compilador
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++20, nunca -std=gnu++20

# ─────────────────────────────────────────────────────────────────────────────
# Automação Qt — DEVE vir antes de qualquer add_executable / add_library
# MOC  : processa Q_OBJECT, Q_SIGNALS, Q_SLOTS
# UIC  : compila .ui → ui_*.h
# RCC  : embute recursos .qrc → binário
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Diretório de busca automática do UIC (caso existam .ui gerados)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ViewLayer/ui")

# ─────────────────────────────────────────────────────────────────────────────
# Dependências externas
# ─────────────────────────────────────────────────────────────────────────────

# Qt6 — módulos exigidos pelos headers analisados:
#   Core            → QObject, QString, QMutex, QFileSystemWatcher, QThreadPool,
#                      QRunnable, QCache, QSet, QFile, QDir, QSizeF …
#   Widgets         → QMainWindow, QWidget, QPixmap, QImage …
#   WebEngineWidgets → QWebEnginePage (EBookEngine)
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Concurrent
    Widgets
    WebEngineWidgets
    DBus
    Svg
    Quick               # Motor QML — CasualModeView
    QuickWidgets        # QQuickWidget — embebe QML dentro do QWidget app
    QuickControls2      # Popup, TabBar, ScrollView, etc.
    Qml                 # QML_ELEMENT / QML_SINGLETON
)

# Poppler-Qt6 — usado por PDFEngine e RenderWorker
# Tentativa 1: CMake nativo do Poppler (instalado via pacote dev moderno)
find_package(Poppler QUIET COMPONENTS Qt6)

if(NOT Poppler_FOUND)
  # Tentativa 2: pkg-config (Ubuntu/Debian: libpoppler-qt6-dev)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Poppler REQUIRED IMPORTED_TARGET poppler-qt6)
  # Cria alias para manter o target_link_libraries uniforme abaixo
  add_library(Poppler::Qt6 ALIAS PkgConfig::Poppler)
  message(STATUS "Poppler encontrado via pkg-config")
else()
  message(STATUS "Poppler encontrado via CMake config: ${Poppler_VERSION}")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Executável principal
# ─────────────────────────────────────────────────────────────────────────────
add_executable(l-reader)

# ─────────────────────────────────────────────────────────────────────────────
# Fontes organizadas por módulo via target_sources
# Cada bloco espelha um subdiretório em src/
# ─────────────────────────────────────────────────────────────────────────────

# ── Ponto de entrada ──────────────────────────────────────────────────────────
target_sources(l-reader PRIVATE
    src/main.cpp
)

# ── DocumentEngine ────────────────────────────────────────────────────────────
# Headers públicos declarados como FILE_SET para instalação futura
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/DocumentEngine/document_engine.hpp
        src/DocumentEngine/pdf_engine.hpp
        src/DocumentEngine/ebook_engine.hpp
        src/DocumentEngine/markdown_engine.hpp
        src/DocumentEngine/toc_worker.hpp
        src/DocumentEngine/pdf_bookmark_manager.hpp
        src/DocumentEngine/document_identity.hpp
        src/DocumentEngine/highlight_entry.hpp
        src/DocumentEngine/highlight_manager.hpp
        src/ViewLayer/bookmark_item_widget.hpp
    PRIVATE
        src/DocumentEngine/document_engine.cpp
        src/DocumentEngine/pdf_engine.cpp
        src/DocumentEngine/ebook_engine.cpp
        src/DocumentEngine/markdown_engine.cpp
        src/DocumentEngine/toc_worker.cpp
        src/DocumentEngine/document_identity.cpp
        src/DocumentEngine/pdf_bookmark_manager.cpp
        src/DocumentEngine/highlight_manager.cpp
        src/ViewLayer/bookmark_item_widget.cpp
)

# ── ModeManager ───────────────────────────────────────────────────────────────
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/ModeManager/reading_mode.hpp
        src/ModeManager/mode_manager.hpp
        src/ModeManager/standard_mode.hpp
        src/ModeManager/study_mode.hpp
        src/ModeManager/casual_mode.hpp
    PRIVATE
        src/ModeManager/mode_manager.cpp
)

# ── Persistence ───────────────────────────────────────────────────────────────
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/Persistence/sidecar_manager.hpp
    PRIVATE
        src/Persistence/sidecar_manager.cpp
)

# ── RenderSubsystem ───────────────────────────────────────────────────────────
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/RenderSubsystem/render_worker.hpp
        src/RenderSubsystem/page_cache.hpp
    PRIVATE
        src/RenderSubsystem/page_cache.cpp
)

# ── ViewLayer ─────────────────────────────────────────────────────────────────
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/ViewLayer/main_window.hpp
        src/ViewLayer/pdf_canvas_view.hpp
    PRIVATE
        src/ViewLayer/main_window.cpp
        src/ViewLayer/main_window_ui.cpp
        src/ViewLayer/main_window_document.cpp
        src/ViewLayer/main_window_scroll.cpp
        src/ViewLayer/main_window_page.cpp
        src/ViewLayer/main_window_epub.cpp
        src/ViewLayer/main_window_toc.cpp
        src/ViewLayer/main_window_bookmarks.cpp
        src/ViewLayer/main_window_gallery.cpp
        src/ViewLayer/main_window_search.cpp
        src/ViewLayer/pdf_canvas_view.cpp
)

# ── Recursos embebidos (.qrc) ─────────────────────────────────────────────────
target_sources(l-reader PRIVATE src/resources.qrc)

# ── CasualMode — Controller QObject exposto ao QML ────────────────────────────
target_sources(l-reader
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES
        src/CasualMode/CasualModeController.h
        src/CasualMode/CasualModeWidget.h      # Q_OBJECT → precisa de .cpp para vtable
    PRIVATE
        src/CasualMode/CasualModeController.cpp
        src/CasualMode/CasualModeWidget.cpp    # unidade mínima que activa o AUTOMOC
)

# ── QML Module — Modo Casual (requer Qt 6.4+) ─────────────────────────────────
# Agrupa os ficheiros QML num módulo com URI "LReader.Casual".
# O motor QML resolve os imports e embute os ficheiros no binário via RCC.
qt_add_qml_module(l-reader
    URI            LReader.Casual
    VERSION        1.0
    RESOURCE_PREFIX /LReader
    QML_FILES
        src/qml/casual/CasualModeView.qml
        src/qml/casual/CasualModeHeader.qml
        src/qml/casual/TextSettingsPopover.qml
        src/qml/casual/CasualModeFooter.qml
        src/qml/casual/CasualModeSidebar.qml
    NO_PLUGIN   # Embute o módulo directo no executável; sem .so separado
)

# ─────────────────────────────────────────────────────────────────────────────
# Diretórios de include
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(l-reader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # Cada módulo pode ser incluído diretamente:
    #   #include "DocumentEngine/document_engine.hpp"
    # ou, dentro do próprio módulo:
    #   #include "pdf_engine.hpp"
)

# ─────────────────────────────────────────────────────────────────────────────
# Link de dependências
# ─────────────────────────────────────────────────────────────────────────────
target_link_libraries(l-reader PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Poppler::Qt6
    Qt6::DBus
    Qt6::Svg
    Qt6::Qml              # QML_ELEMENT, QQmlContext, QQmlApplicationEngine
    Qt6::Quick            # motor QML (QQuickItem, QQuickWindow, etc.)
    Qt6::QuickWidgets     # QQuickWidget — hospeda CasualModeView no QWidget app
    Qt6::QuickControls2   # Popup, TabBar, ScrollView, ScrollBar
)

# ─────────────────────────────────────────────────────────────────────────────
# Otimizações de compilação — alta performance para o leitor
# ─────────────────────────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(l-reader PRIVATE
        # Nível máximo de otimização sem comprometer a reprodutibilidade do debug
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>        # instruções SIMD da máquina local
        $<$<CONFIG:Release>:-funroll-loops>        # desenrola loops pequenos
        $<$<CONFIG:Release>:-ffast-math>           # relaxa IEEE 754 para throughput
        $<$<CONFIG:Release>:-fomit-frame-pointer>  # libera um registrador extra
        $<$<CONFIG:Release>:-DNDEBUG>              # desativa Q_ASSERT / assert()

        # RelWithDebInfo: otimizado + símbolos (profiling / perf)
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-g>

        # Debug: sem otimização, máxima rastreabilidade
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>

        # Warnings sempre ativos — ajudam a pegar bugs cedo
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wno-unused-parameter     # Qt gera muitos desses em slots
    )
elseif(MSVC)
  target_compile_options(l-reader PRIVATE
        $<$<CONFIG:Release>:/O2 /GL /Gy /fp:fast /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi>
        /W4
        /wd4100   # unreferenced formal parameter (equivalente ao Wno-unused-parameter)
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# LTO / IPO — Link-Time Optimization (Release apenas)
# Reduz tamanho do binário e permite inlining cross-TU
# ─────────────────────────────────────────────────────────────────────────────
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
if(_ipo_ok)
  set_target_properties(l-reader PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
  message(STATUS "LTO/IPO habilitado para Release")
else()
  message(WARNING "LTO/IPO não suportado: ${_ipo_msg}")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Definições de pré-processador
# ─────────────────────────────────────────────────────────────────────────────
target_compile_definitions(l-reader PRIVATE
    # Desativa APIs Qt depreciadas anteriores ao Qt 6.4
    QT_DISABLE_DEPRECATED_BEFORE=0x060400

    # Ativa checagens extras do Qt em Debug (iteradores inválidos, etc.)
    $<$<CONFIG:Debug>:QT_STRICT_ITERATORS>
    $<$<CONFIG:Debug>:QT_NO_CAST_FROM_ASCII>
)

# ─────────────────────────────────────────────────────────────────────────────
# Unity Build (Jumbo Build) — opcional, acelera compilações incrementais
# Ative com: cmake -DENABLE_UNITY_BUILD=ON ..
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_UNITY_BUILD "Agrupa TUs por módulo (unity/jumbo build)" OFF)
if(ENABLE_UNITY_BUILD)
  set_target_properties(l-reader PROPERTIES
        UNITY_BUILD             ON
        UNITY_BUILD_BATCH_SIZE  8   # TUs por arquivo unity gerado
    )
  message(STATUS "Unity build habilitado (batch=8)")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Resumo de configuração (impresso ao final do cmake)
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "──────────────────────────────────────────")
message(STATUS " l-reader ${PROJECT_VERSION}")
message(STATUS " Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS " Compilador : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Qt6        : ${Qt6_VERSION}")
message(STATUS "──────────────────────────────────────────")
