// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/CasualModeWidget.cpp  —  l-reader · Modo Casual
//
// Ficheiro de compilação mínimo para CasualModeWidget.
//
// Porquê este ficheiro existe:
//   CasualModeWidget declara Q_OBJECT no seu header.  O AUTOMOC do Qt gera
//   automaticamente o ficheiro moc_CasualModeWidget.cpp com a implementação
//   da vtable, dos meta-métodos e do mecanismo de sinais/slots.  No entanto,
//   para uma classe header-only esse ficheiro gerado NUNCA é compilado — o
//   linker não encontra a vtable e emite "undefined reference to vtable for
//   CasualModeWidget".
//
//   A solução canónica é ter um .cpp que inclua o header, fazendo o AUTOMOC
//   reconhecer a unidade de compilação e ligar o output do MOC correctamente.
// ─────────────────────────────────────────────────────────────────────────────
#include "CasualModeWidget.h"
