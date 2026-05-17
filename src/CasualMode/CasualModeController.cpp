// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/CasualModeController.cpp  —  l-reader · Modo Casual
// ─────────────────────────────────────────────────────────────────────────────
#include "CasualModeController.h"

#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Paleta de cores — definição da constexpr declarada no header
// ─────────────────────────────────────────────────────────────────────────────
constexpr CasualModeController::Palette CasualModeController::kPalettes[4];

// ─────────────────────────────────────────────────────────────────────────────
// Texto mockado — Lorem Ipsum rico em HTML/RichText.
// Dividido em dois blocos para alimentar a página esquerda e a direita
// do layout bicolumnar.  Gerado uma única vez (static local).
// ─────────────────────────────────────────────────────────────────────────────
const QString& CasualModeController::staticLeftHtml()
{
    static const QString s_html = QString::fromUtf8(
        R"HTML(
<p style='margin:0 0 1.2em 0;'>
<span style='font-size:1.6em; font-weight:600; letter-spacing:-0.02em;'>Capítulo III</span><br/>
<span style='font-size:1.15em; color:#888; letter-spacing:0.08em; text-transform:uppercase;'>O Labirinto Infinito</span>
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
No princípio era o Verbo, e o Verbo era a Biblioteca. Adso caminhava
pelos corredores silenciosos do scriptorium enquanto a luz da manhã
atravessava os vitrais coloridos, projectando padrões de ouro e azul
sobre os pergaminhos dispostos nas longas bancadas de carvalho escuro.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
Frei Guilherme de Baskerville seguia-o de perto, os olhos miúdos e
penetrantes percorrendo as prateleiras com aquela intensidade peculiar
que tanto intimidava os monges mais jovens. <em>«Há aqui um livro»</em>,
murmurou finalmente, <em>«que ninguém deve ler, mas que todos temem.»</em>
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
A abadia de Melk erguia-se sobre a rocha como uma coroa de pedra.
Os seus muros guardavam séculos de saber acumulado, copiado,
comentado e muitas vezes incompreendido. Os monges eram guardiões
zelosos de um tesouro que poucos, fora das suas ordens, poderiam
sequer imaginar na sua plenitude e complexidade.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
«Venerable Jorge», disse Guilherme, virando-se para o ancião cego
que rezava no canto mais sombrio do scriptorium, «que me diz da
morte de Adelmo? Era um iluminador de grande talento.» O ancião
levantou lentamente a cabeça, e nos seus olhos opacos Adso julgou
ver uma sombra de algo que não era bem tristeza.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
O inverno daquele ano de graça de 1327 havia chegado cedo e com
inusitada ferocidade. A neve cobria os campos e os bosques que
rodeavam o mosteiro, e os ventos cortantes do norte tornavam
qualquer viagem um exercício de penitência involuntária. Era, por
isso, com alguma surpresa que Adso observara a chegada de tantos
visitantes illustres na véspera de Natal.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
O scriptorium era o coração pulsante da comunidade, o lugar onde o
passado se tornava permanente e o efémero adquiria a gravidade da
pedra. Dezenas de mãos trabalhavam em silêncio, penas de ganso
arranhando pacientemente o vitelo tratado. O cheiro a tinta e a
cera de abelha misturava-se com o incenso que continuamente ardia
nos turíbulos de bronze pendurados das abóbadas.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
Berengar, o sub-bibliotecário, aproximou-se com passos hesitantes.
Era um homem novo, de feições aguçadas e olhar evasivo, que parecia
carregar sempre consigo o peso de um segredo que o sufocava.
<em>«Irmão Guilherme»</em>, disse em voz baixa, <em>«há coisas nesta
abadia que seria melhor não investigar.»</em>
</p>
        )HTML"
    );
    return s_html;
}

const QString& CasualModeController::staticRightHtml()
{
    // Raw string literal (C++11) — aspas e caracteres especiais sem necessidade de escape
    static const QString s_html = QString::fromUtf8(
        R"HTML(
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
Guilherme olhou para ele com aquela expressão de amável curiosidade
que Adso já havia aprendido a reconhecer como sinal de interesse
máximo. <em>«É precisamente o que um homem sensato diria quando
deseja dissuadir um investigador»</em>, respondeu serenamente,
<em>«o que torna a investigação absolutamente necessária.»</em>
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
A biblioteca ocupava o andar superior da torre oriental. Nenhum monge,
excepto o bibliotecário e o seu assistente, tinha acesso directo às
suas salas. Os livros desciam ao scriptorium mediante pedido formal
ao Abade, e mesmo assim apenas os volumes considerados adequados ao
estudo de cada irmão em particular eram concedidos.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
«É um labirinto», explicou Malaquias, o bibliotecário, com uma
solenidade que beirava o orgulho. «Foi construído assim
propositadamente, para que apenas os iniciados possam navegar os
seus corredores sem se perderem nas trevas.» Havia nas suas palavras
uma satisfação que Adso achou perturbante, como se o caos fosse
uma forma de poder sobre aqueles que nele se perdiam.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
Naquela noite, Adso sonhou com livros. Via-os dispostos em filas
intermináveis que se estendiam para além do horizonte, cada
volume brilhando com uma luz própria, suave e azulada como a
chama de uma vela vista através de um vidro. Nas lombadas
gravadas a ouro lia nomes que não reconhecia em línguas que nunca
aprendera, e ainda assim compreendia o seu significado com uma
clareza quase dolorosa.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
De manhã, encontraram o segundo corpo. Jazia ao pé da torre, na
mesma posição que Adelmo, os dedos ainda crispados em torno de
nada, o rosto voltado para o céu cinzento de Janeiro. Guilherme
ajoelhou-se e examinou as mãos com a sua lente de cristal.
«Manchas negras nos dedos e na boca», disse pensativamente.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
O Abade Abbone convocou o capítulo ao meio-dia. Sentado no seu
trono entalhado, as mãos juntas sobre o hábito negro, falou com
a voz cuidadosamente modulada de quem está habituado à autoridade.
<em>«Deus prova a fé dos seus servidores de maneiras que escapam à
nossa compreensão»</em>, disse. <em>«Rezemos pelos nossos irmãos
partidos e prossigamos o trabalho que lhes foi confiado.»</em>
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
Mas Guilherme não rezou. Ficou na sua cela, rodeado de notas e
fragmentos de pergaminho, construindo metodicamente o mapa do
labirinto a partir dos dados que recolhera durante o dia. Adso
observava-o em silêncio, fascinado pela forma como aquela mente
singular transformava o caos em ordem, o mistério em geometria.
</p>
<p style='margin:0 0 1em 0; text-indent:1.8em;'>
«A verdade», disse finalmente Guilherme, sem levantar os olhos
das suas anotações, «não está nos livros que possuímos, mas nos
livros que alguém não quer que leiamos. E é sempre nesses que
encontramos as respostas mais perigosas, as únicas que valem a
pena procurar.»
</p>
        )HTML"
    );
    return s_html;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
CasualModeController::CasualModeController(QObject* parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Cores derivadas do tema
// ─────────────────────────────────────────────────────────────────────────────
QString CasualModeController::bgColor() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].bg);
}

QString CasualModeController::textColor() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].text);
}

QString CasualModeController::headerBg() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].header);
}

QString CasualModeController::accentColor() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].accent);
}

QString CasualModeController::borderColor() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].border);
}

QString CasualModeController::mutedColor() const noexcept
{
    return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].muted);
}

// ─────────────────────────────────────────────────────────────────────────────
// Conteúdo das páginas (mockado)
// ─────────────────────────────────────────────────────────────────────────────
QString CasualModeController::leftPageHtml() const noexcept
{
    return staticLeftHtml();
}

QString CasualModeController::rightPageHtml() const noexcept
{
    return staticRightHtml();
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setTheme(int theme)
{
    const auto t = static_cast<Theme>(std::clamp(theme, 0, 3));
    if (m_theme == t) return;
    m_theme = t;
    emit themeChanged();
}

void CasualModeController::setChapterTitle(const QString& title)
{
    if (m_chapterTitle == title) return;
    m_chapterTitle = title;
    emit chapterTitleChanged();
}

void CasualModeController::setReadingProgress(qreal progress)
{
    const qreal p = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_readingProgress, p)) return;
    m_readingProgress = p;
    emit readingProgressChanged();
}

void CasualModeController::setCurrentPage(int page)
{
    const int p = std::clamp(page, 1, m_totalPages);
    if (m_currentPage == p) return;
    m_currentPage = p;
    setReadingProgress(static_cast<qreal>(p) / m_totalPages);
    emit currentPageChanged();
}

void CasualModeController::setFontSize(int size)
{
    const int s = std::clamp(size, 10, 32);
    if (m_fontSize == s) return;
    m_fontSize = s;
    emit fontSizeChanged();
}

void CasualModeController::setColumnMargin(int margin)
{
    const int m = std::clamp(margin, 24, 160);
    if (m_columnMargin == m) return;
    m_columnMargin = m;
    emit columnMarginChanged();
}

void CasualModeController::setLineSpacing(int spacing)
{
    const int s = std::clamp(spacing, 0, 24);
    if (m_lineSpacing == s) return;
    m_lineSpacing = s;
    emit lineSpacingChanged();
}

void CasualModeController::setSidebarOpen(bool open)
{
    if (m_sidebarOpen == open) return;
    m_sidebarOpen = open;
    emit sidebarOpenChanged();
}

void CasualModeController::setSearchOpen(bool open)
{
    if (m_searchOpen == open) return;
    m_searchOpen = open;
    emit searchOpenChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Invokable — acções de UI
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::increaseFontSize()  { setFontSize(m_fontSize + 1); }
void CasualModeController::decreaseFontSize()  { setFontSize(m_fontSize - 1); }
void CasualModeController::increaseMargin()    { setColumnMargin(m_columnMargin + 8); }
void CasualModeController::decreaseMargin()    { setColumnMargin(m_columnMargin - 8); }
void CasualModeController::toggleSidebar()     { setSidebarOpen(!m_sidebarOpen); }
void CasualModeController::toggleSearch()      { setSearchOpen(!m_searchOpen); }
