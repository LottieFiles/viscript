// Parses TeX math into nodes: the symbols, operators, delimiters, accents, fractions, roots,
// scripts and environments of the supported subset.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

#include "visTex.h"

namespace vis::tex::detail
{

struct Symbol
{
    const char* name;
    const char* text;
    Class cls;
    Variant variant;
};

// The symbols a command names. Lowercase Greek is set in italic like Latin letters, the rest
// upright, as TeX sets them.
static constexpr Symbol SYMBOLS[] = {
    {"alpha", "α", Class::Ord, Variant::Italic},
    {"beta", "β", Class::Ord, Variant::Italic},
    {"gamma", "γ", Class::Ord, Variant::Italic},
    {"delta", "δ", Class::Ord, Variant::Italic},
    {"epsilon", "ϵ", Class::Ord, Variant::Italic},
    {"varepsilon", "ε", Class::Ord, Variant::Italic},
    {"zeta", "ζ", Class::Ord, Variant::Italic},
    {"eta", "η", Class::Ord, Variant::Italic},
    {"theta", "θ", Class::Ord, Variant::Italic},
    {"vartheta", "ϑ", Class::Ord, Variant::Italic},
    {"iota", "ι", Class::Ord, Variant::Italic},
    {"kappa", "κ", Class::Ord, Variant::Italic},
    {"lambda", "λ", Class::Ord, Variant::Italic},
    {"mu", "μ", Class::Ord, Variant::Italic},
    {"nu", "ν", Class::Ord, Variant::Italic},
    {"xi", "ξ", Class::Ord, Variant::Italic},
    {"omicron", "ο", Class::Ord, Variant::Italic},
    {"pi", "π", Class::Ord, Variant::Italic},
    {"varpi", "ϖ", Class::Ord, Variant::Italic},
    {"rho", "ρ", Class::Ord, Variant::Italic},
    {"varrho", "ϱ", Class::Ord, Variant::Italic},
    {"sigma", "σ", Class::Ord, Variant::Italic},
    {"varsigma", "ς", Class::Ord, Variant::Italic},
    {"tau", "τ", Class::Ord, Variant::Italic},
    {"upsilon", "υ", Class::Ord, Variant::Italic},
    {"phi", "ϕ", Class::Ord, Variant::Italic},
    {"varphi", "φ", Class::Ord, Variant::Italic},
    {"chi", "χ", Class::Ord, Variant::Italic},
    {"psi", "ψ", Class::Ord, Variant::Italic},
    {"omega", "ω", Class::Ord, Variant::Italic},
    {"Gamma", "Γ", Class::Ord, Variant::Upright},
    {"Delta", "Δ", Class::Ord, Variant::Upright},
    {"Theta", "Θ", Class::Ord, Variant::Upright},
    {"Lambda", "Λ", Class::Ord, Variant::Upright},
    {"Xi", "Ξ", Class::Ord, Variant::Upright},
    {"Pi", "Π", Class::Ord, Variant::Upright},
    {"Sigma", "Σ", Class::Ord, Variant::Upright},
    {"Upsilon", "Υ", Class::Ord, Variant::Upright},
    {"Phi", "Φ", Class::Ord, Variant::Upright},
    {"Psi", "Ψ", Class::Ord, Variant::Upright},
    {"Omega", "Ω", Class::Ord, Variant::Upright},

    {"pm", "±", Class::Bin, Variant::Upright},
    {"mp", "∓", Class::Bin, Variant::Upright},
    {"times", "×", Class::Bin, Variant::Upright},
    {"div", "÷", Class::Bin, Variant::Upright},
    {"cdot", "⋅", Class::Bin, Variant::Upright},
    {"ast", "∗", Class::Bin, Variant::Upright},
    {"star", "⋆", Class::Bin, Variant::Upright},
    {"circ", "∘", Class::Bin, Variant::Upright},
    {"bullet", "∙", Class::Bin, Variant::Upright},
    {"cap", "∩", Class::Bin, Variant::Upright},
    {"cup", "∪", Class::Bin, Variant::Upright},
    {"wedge", "∧", Class::Bin, Variant::Upright},
    {"land", "∧", Class::Bin, Variant::Upright},
    {"vee", "∨", Class::Bin, Variant::Upright},
    {"lor", "∨", Class::Bin, Variant::Upright},
    {"setminus", "∖", Class::Bin, Variant::Upright},
    {"oplus", "⊕", Class::Bin, Variant::Upright},
    {"ominus", "⊖", Class::Bin, Variant::Upright},
    {"otimes", "⊗", Class::Bin, Variant::Upright},
    {"odot", "⊙", Class::Bin, Variant::Upright},
    {"dagger", "†", Class::Bin, Variant::Upright},
    {"ddagger", "‡", Class::Bin, Variant::Upright},

    {"le", "≤", Class::Rel, Variant::Upright},
    {"leq", "≤", Class::Rel, Variant::Upright},
    {"ge", "≥", Class::Rel, Variant::Upright},
    {"geq", "≥", Class::Rel, Variant::Upright},
    {"ne", "≠", Class::Rel, Variant::Upright},
    {"neq", "≠", Class::Rel, Variant::Upright},
    {"approx", "≈", Class::Rel, Variant::Upright},
    {"equiv", "≡", Class::Rel, Variant::Upright},
    {"sim", "∼", Class::Rel, Variant::Upright},
    {"simeq", "≃", Class::Rel, Variant::Upright},
    {"cong", "≅", Class::Rel, Variant::Upright},
    {"propto", "∝", Class::Rel, Variant::Upright},
    {"ll", "≪", Class::Rel, Variant::Upright},
    {"gg", "≫", Class::Rel, Variant::Upright},
    {"prec", "≺", Class::Rel, Variant::Upright},
    {"succ", "≻", Class::Rel, Variant::Upright},
    {"subset", "⊂", Class::Rel, Variant::Upright},
    {"supset", "⊃", Class::Rel, Variant::Upright},
    {"subseteq", "⊆", Class::Rel, Variant::Upright},
    {"supseteq", "⊇", Class::Rel, Variant::Upright},
    {"in", "∈", Class::Rel, Variant::Upright},
    {"notin", "∉", Class::Rel, Variant::Upright},
    {"ni", "∋", Class::Rel, Variant::Upright},
    {"perp", "⊥", Class::Rel, Variant::Upright},
    {"parallel", "∥", Class::Rel, Variant::Upright},
    {"mid", "∣", Class::Rel, Variant::Upright},
    {"vdash", "⊢", Class::Rel, Variant::Upright},
    {"models", "⊨", Class::Rel, Variant::Upright},
    {"coloneqq", "≔", Class::Rel, Variant::Upright},
    {"to", "→", Class::Rel, Variant::Upright},
    {"rightarrow", "→", Class::Rel, Variant::Upright},
    {"leftarrow", "←", Class::Rel, Variant::Upright},
    {"gets", "←", Class::Rel, Variant::Upright},
    {"leftrightarrow", "↔", Class::Rel, Variant::Upright},
    {"Rightarrow", "⇒", Class::Rel, Variant::Upright},
    {"Leftarrow", "⇐", Class::Rel, Variant::Upright},
    {"Leftrightarrow", "⇔", Class::Rel, Variant::Upright},
    {"implies", "⟹", Class::Rel, Variant::Upright},
    {"impliedby", "⟸", Class::Rel, Variant::Upright},
    {"iff", "⟺", Class::Rel, Variant::Upright},
    {"mapsto", "↦", Class::Rel, Variant::Upright},
    {"longrightarrow", "⟶", Class::Rel, Variant::Upright},
    {"longleftarrow", "⟵", Class::Rel, Variant::Upright},
    {"uparrow", "↑", Class::Rel, Variant::Upright},
    {"downarrow", "↓", Class::Rel, Variant::Upright},
    {"rightleftharpoons", "⇌", Class::Rel, Variant::Upright},

    {"infty", "∞", Class::Ord, Variant::Upright},
    {"partial", "∂", Class::Ord, Variant::Upright},
    {"nabla", "∇", Class::Ord, Variant::Upright},
    {"forall", "∀", Class::Ord, Variant::Upright},
    {"exists", "∃", Class::Ord, Variant::Upright},
    {"emptyset", "∅", Class::Ord, Variant::Upright},
    {"varnothing", "∅", Class::Ord, Variant::Upright},
    {"hbar", "ℏ", Class::Ord, Variant::Upright},
    {"ell", "ℓ", Class::Ord, Variant::Upright},
    {"prime", "′", Class::Ord, Variant::Upright},
    {"angle", "∠", Class::Ord, Variant::Upright},
    {"triangle", "△", Class::Ord, Variant::Upright},
    {"neg", "¬", Class::Ord, Variant::Upright},
    {"lnot", "¬", Class::Ord, Variant::Upright},
    {"Re", "ℜ", Class::Ord, Variant::Upright},
    {"Im", "ℑ", Class::Ord, Variant::Upright},
    {"aleph", "ℵ", Class::Ord, Variant::Upright},
    {"top", "⊤", Class::Ord, Variant::Upright},
    {"bot", "⊥", Class::Ord, Variant::Upright},
    {"degree", "°", Class::Ord, Variant::Upright},
    {"imath", "ı", Class::Ord, Variant::Italic},
    {"jmath", "ȷ", Class::Ord, Variant::Italic},
    {"ldots", "…", Class::Inner, Variant::Upright},
    {"dots", "…", Class::Inner, Variant::Upright},
    {"cdots", "⋯", Class::Inner, Variant::Upright},
    {"vdots", "⋮", Class::Ord, Variant::Upright},
    {"ddots", "⋱", Class::Inner, Variant::Upright},
    {"#", "#", Class::Ord, Variant::Upright},
    {"$", "$", Class::Ord, Variant::Upright},
    {"%", "%", Class::Ord, Variant::Upright},
    {"&", "&", Class::Ord, Variant::Upright},
    {"_", "_", Class::Ord, Variant::Upright},
    {"backslash", "\\", Class::Ord, Variant::Upright},
    {"|", "‖", Class::Ord, Variant::Upright},
    {"vert", "|", Class::Ord, Variant::Upright},
    {"Vert", "‖", Class::Ord, Variant::Upright},
    {"{", "{", Class::Open, Variant::Upright},
    {"}", "}", Class::Close, Variant::Upright},
    {"lbrace", "{", Class::Open, Variant::Upright},
    {"rbrace", "}", Class::Close, Variant::Upright},
    {"lvert", "|", Class::Open, Variant::Upright},
    {"rvert", "|", Class::Close, Variant::Upright},
    {"lVert", "‖", Class::Open, Variant::Upright},
    {"rVert", "‖", Class::Close, Variant::Upright},
    {"colon", ":", Class::Punct, Variant::Upright},
};

struct Operator
{
    const char* name;
    const char* text;
    uint8_t flags;
};

// Big operators are glyphs drawn larger in display style; the others are upright words.
static constexpr Operator OPERATORS[] = {
    {"sum", "∑", FlagLarge | FlagLimits},
    {"prod", "∏", FlagLarge | FlagLimits},
    {"coprod", "∐", FlagLarge | FlagLimits},
    {"bigcup", "⋃", FlagLarge | FlagLimits},
    {"bigcap", "⋂", FlagLarge | FlagLimits},
    {"bigvee", "⋁", FlagLarge | FlagLimits},
    {"bigwedge", "⋀", FlagLarge | FlagLimits},
    {"bigoplus", "⨁", FlagLarge | FlagLimits},
    {"bigotimes", "⨂", FlagLarge | FlagLimits},
    {"bigodot", "⨀", FlagLarge | FlagLimits},
    {"int", "∫", FlagLarge | FlagIntegral},
    {"iint", "∬", FlagLarge | FlagIntegral},
    {"iiint", "∭", FlagLarge | FlagIntegral},
    {"oint", "∮", FlagLarge | FlagIntegral},
    {"lim", "lim", FlagLimits},
    {"liminf", "lim inf", FlagLimits},
    {"limsup", "lim sup", FlagLimits},
    {"max", "max", FlagLimits},
    {"min", "min", FlagLimits},
    {"sup", "sup", FlagLimits},
    {"inf", "inf", FlagLimits},
    {"det", "det", FlagLimits},
    {"gcd", "gcd", FlagLimits},
    {"Pr", "Pr", FlagLimits},
    {"sin", "sin", 0},
    {"cos", "cos", 0},
    {"tan", "tan", 0},
    {"cot", "cot", 0},
    {"sec", "sec", 0},
    {"csc", "csc", 0},
    {"arcsin", "arcsin", 0},
    {"arccos", "arccos", 0},
    {"arctan", "arctan", 0},
    {"sinh", "sinh", 0},
    {"cosh", "cosh", 0},
    {"tanh", "tanh", 0},
    {"coth", "coth", 0},
    {"log", "log", 0},
    {"ln", "ln", 0},
    {"lg", "lg", 0},
    {"exp", "exp", 0},
    {"dim", "dim", 0},
    {"ker", "ker", 0},
    {"deg", "deg", 0},
    {"arg", "arg", 0},
    {"hom", "hom", 0},
};

struct Delimiter
{
    const char* name;
    Delim delim;
};

// The delimiters \left, \right and \big take, as characters and as commands.
static constexpr Delimiter DELIMITERS[] = {
    {"(", Delim::LeftParen},
    {")", Delim::RightParen},
    {"[", Delim::LeftBracket},
    {"]", Delim::RightBracket},
    {"|", Delim::Vert},
    {"/", Delim::Slash},
    {"<", Delim::LeftAngle},
    {">", Delim::RightAngle},
    {".", Delim::None},
    {"\\{", Delim::LeftBrace},
    {"\\}", Delim::RightBrace},
    {"\\lbrace", Delim::LeftBrace},
    {"\\rbrace", Delim::RightBrace},
    {"\\|", Delim::DoubleVert},
    {"\\vert", Delim::Vert},
    {"\\Vert", Delim::DoubleVert},
    {"\\lvert", Delim::Vert},
    {"\\rvert", Delim::Vert},
    {"\\lVert", Delim::DoubleVert},
    {"\\rVert", Delim::DoubleVert},
    {"\\langle", Delim::LeftAngle},
    {"\\rangle", Delim::RightAngle},
    {"\\lfloor", Delim::LeftFloor},
    {"\\rfloor", Delim::RightFloor},
    {"\\lceil", Delim::LeftCeil},
    {"\\rceil", Delim::RightCeil},
    {"\\backslash", Delim::Backslash},
};

struct Named
{
    const char* name;
    uint8_t value;
};

static constexpr Named ACCENTS[] = {
    {"hat", static_cast<uint8_t>(Accent::Hat)},
    {"check", static_cast<uint8_t>(Accent::Check)},
    {"tilde", static_cast<uint8_t>(Accent::Tilde)},
    {"bar", static_cast<uint8_t>(Accent::Bar)},
    {"vec", static_cast<uint8_t>(Accent::Vec)},
    {"dot", static_cast<uint8_t>(Accent::Dot)},
    {"ddot", static_cast<uint8_t>(Accent::Ddot)},
    {"widehat", static_cast<uint8_t>(Accent::WideHat)},
    {"widetilde", static_cast<uint8_t>(Accent::WideTilde)},
    {"overline", static_cast<uint8_t>(Accent::Overline)},
    {"underline", static_cast<uint8_t>(Accent::Underline)},
};

static constexpr Named FONTS[] = {
    {"mathrm", static_cast<uint8_t>(Variant::Upright)},
    {"mathit", static_cast<uint8_t>(Variant::Italic)},
    {"mathbf", static_cast<uint8_t>(Variant::Bold)},
    {"boldsymbol", static_cast<uint8_t>(Variant::Bold)},
    {"mathbb", static_cast<uint8_t>(Variant::DoubleStruck)},
    {"mathcal", static_cast<uint8_t>(Variant::Script)},
};

// Commands whose argument is set as text, in the alphabet they name.
static constexpr Named TEXTS[] = {
    {"text", static_cast<uint8_t>(Variant::Upright)},
    {"textrm", static_cast<uint8_t>(Variant::Upright)},
    {"textup", static_cast<uint8_t>(Variant::Upright)},
    {"textnormal", static_cast<uint8_t>(Variant::Upright)},
    {"mbox", static_cast<uint8_t>(Variant::Upright)},
    {"textit", static_cast<uint8_t>(Variant::Italic)},
    {"textbf", static_cast<uint8_t>(Variant::Bold)},
};

// Spaces in math units, 18 to an em.
struct Spacing
{
    const char* name;
    float mu;
};

static constexpr Spacing SPACES[] = {
    {",", 3.0f},
    {":", 4.0f},
    {">", 4.0f},
    {";", 5.0f},
    {"!", -3.0f},
    {" ", 6.0f},
    {"thinspace", 3.0f},
    {"medspace", 4.0f},
    {"thickspace", 5.0f},
    {"negthinspace", -3.0f},
    {"enspace", 9.0f},
    {"quad", 18.0f},
    {"qquad", 36.0f},
};

static constexpr Named STYLES[] = {
    {"displaystyle", static_cast<uint8_t>(Level::Display)},
    {"textstyle", static_cast<uint8_t>(Level::Text)},
    {"scriptstyle", static_cast<uint8_t>(Level::Script)},
    {"scriptscriptstyle", static_cast<uint8_t>(Level::ScriptScript)},
};

static constexpr Named ENVIRONMENTS[] = {
    {"matrix", static_cast<uint8_t>(Environment::Matrix)},
    {"pmatrix", static_cast<uint8_t>(Environment::PMatrix)},
    {"bmatrix", static_cast<uint8_t>(Environment::BMatrix)},
    {"Bmatrix", static_cast<uint8_t>(Environment::BraceMatrix)},
    {"vmatrix", static_cast<uint8_t>(Environment::VMatrix)},
    {"Vmatrix", static_cast<uint8_t>(Environment::DoubleVMatrix)},
    {"cases", static_cast<uint8_t>(Environment::Cases)},
    {"aligned", static_cast<uint8_t>(Environment::Aligned)},
};

// The relations \not turns into their negations.
struct Negation
{
    const char* relation;
    const char* text;
};

static constexpr Negation NEGATIONS[] = {
    {"=", "≠"},
    {"<", "≮"},
    {">", "≯"},
    {"\\in", "∉"},
    {"\\equiv", "≢"},
    {"\\sim", "≁"},
    {"\\approx", "≉"},
    {"\\subset", "⊄"},
    {"\\supset", "⊅"},
    {"\\le", "≰"},
    {"\\leq", "≰"},
    {"\\ge", "≱"},
    {"\\geq", "≱"},
};

static constexpr uint8_t FlagPrimed = 128u;
static constexpr uint32_t DepthLimit = 64u;

template<typename T, size_t N>
static const T* _find(const T (&table)[N], const char* name, uint32_t length)
{
    for (auto& entry : table) {
        if (std::strlen(entry.name) == length && std::memcmp(entry.name, name, length) == 0) return &entry;
    }
    return nullptr;
}

uint32_t decode(const char* text, uint32_t length, uint32_t& codepoint) noexcept
{
    codepoint = 0xfffdu;
    if (!length) return 0u;
    auto lead = static_cast<uint8_t>(text[0]);
    uint32_t size = lead < 0x80u ? 1u : (lead >> 5) == 0x6u ? 2u : (lead >> 4) == 0xeu ? 3u : (lead >> 3) == 0x1eu ? 4u : 0u;
    if (!size || size > length) return 1u;
    uint32_t value = size == 1u ? lead : size == 2u ? lead & 0x1fu : size == 3u ? lead & 0x0fu : lead & 0x07u;
    for (auto i = 1u; i < size; i++) {
        auto next = static_cast<uint8_t>(text[i]);
        if ((next & 0xc0u) != 0x80u) return 1u;
        value = (value << 6) | (next & 0x3fu);
    }
    codepoint = value;
    return size;
}

uint32_t encode(uint32_t codepoint, char* output) noexcept
{
    if (codepoint < 0x80u) {
        output[0] = static_cast<char>(codepoint);
        return 1u;
    }
    if (codepoint < 0x800u) {
        output[0] = static_cast<char>(0xc0u | (codepoint >> 6));
        output[1] = static_cast<char>(0x80u | (codepoint & 0x3fu));
        return 2u;
    }
    if (codepoint < 0x10000u) {
        output[0] = static_cast<char>(0xe0u | (codepoint >> 12));
        output[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu));
        output[2] = static_cast<char>(0x80u | (codepoint & 0x3fu));
        return 3u;
    }
    output[0] = static_cast<char>(0xf0u | (codepoint >> 18));
    output[1] = static_cast<char>(0x80u | ((codepoint >> 12) & 0x3fu));
    output[2] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu));
    output[3] = static_cast<char>(0x80u | (codepoint & 0x3fu));
    return 4u;
}

uint32_t alphabet(uint32_t c, Variant variant) noexcept
{
    auto upper = c >= 'A' && c <= 'Z';
    auto lower = c >= 'a' && c <= 'z';
    auto digit = c >= '0' && c <= '9';
    auto greekUpper = c >= 0x391u && c <= 0x3a9u && c != 0x3a2u;
    auto greekLower = c >= 0x3b1u && c <= 0x3c9u;
    switch (variant) {
        case Variant::Italic:
            if (c == 'h') return 0x210eu;
            if (upper) return 0x1d434u + (c - 'A');
            if (lower) return 0x1d44eu + (c - 'a');
            if (greekLower) return 0x1d6fcu + (c - 0x3b1u);
            if (c == 0x3f5u) return 0x1d716u;
            if (c == 0x3d1u) return 0x1d717u;
            if (c == 0x3d5u) return 0x1d719u;
            if (c == 0x3f1u) return 0x1d71au;
            if (c == 0x3d6u) return 0x1d71bu;
            return c;
        case Variant::Bold:
            if (upper) return 0x1d400u + (c - 'A');
            if (lower) return 0x1d41au + (c - 'a');
            if (digit) return 0x1d7ceu + (c - '0');
            if (greekUpper) return 0x1d6a8u + (c - 0x391u);
            if (greekLower) return 0x1d6c2u + (c - 0x3b1u);
            return c;
        case Variant::DoubleStruck: {
            static constexpr uint32_t holes[] = {'C', 0x2102u, 'H', 0x210du, 'N', 0x2115u, 'P', 0x2119u,
                                                 'Q', 0x211au, 'R', 0x211du, 'Z', 0x2124u};
            for (auto i = 0u; i < sizeof(holes) / sizeof(holes[0]); i += 2u) {
                if (holes[i] == c) return holes[i + 1u];
            }
            if (upper) return 0x1d538u + (c - 'A');
            if (lower) return 0x1d552u + (c - 'a');
            if (digit) return 0x1d7d8u + (c - '0');
            return c;
        }
        case Variant::Script: {
            static constexpr uint32_t holes[] = {'B', 0x212cu, 'E', 0x2130u, 'F', 0x2131u, 'H', 0x210bu,
                                                 'I', 0x2110u, 'L', 0x2112u, 'M', 0x2133u, 'R', 0x211bu,
                                                 'e', 0x212fu, 'g', 0x210au, 'o', 0x2134u};
            for (auto i = 0u; i < sizeof(holes) / sizeof(holes[0]); i += 2u) {
                if (holes[i] == c) return holes[i + 1u];
            }
            if (upper) return 0x1d49cu + (c - 'A');
            if (lower) return 0x1d4b6u + (c - 'a');
            return c;
        }
        default: return c;
    }
}

Parse::~Parse()
{
    delete[] nodes;
    delete[] strings;
}

namespace
{

enum struct Stop : uint8_t
{
    End = 0,
    Brace,
    Bracket,
    Right,
    Cell
};

// What ended the list of a cell.
enum struct Ended : uint8_t
{
    Source = 0,
    Ampersand,
    Row,
    Environment
};

struct Parser
{
    const char* source;
    uint32_t size;
    Parse& out;
    char* error;
    uint32_t errorSize;
    uint32_t pos = 0;
    uint32_t depth = 0;
    bool failed = false;
    Ended ended = Ended::Source;
    // The alphabet of letters, as \mathrm and its kin set it; Italic is the math default.
    Variant variant = Variant::Italic;
    bool fixed = false;

    Parser(const char* source, uint32_t size, Parse& out, char* error, uint32_t errorSize)
        : source(source), size(size), out(out), error(error), errorSize(errorSize)
    {
    }

    // The 1-based character of the byte at offset.
    uint32_t character(uint32_t offset) const
    {
        uint32_t count = 1u;
        for (auto i = 0u; i < offset && i < size; i++) {
            if ((static_cast<uint8_t>(source[i]) & 0xc0u) != 0x80u) count++;
        }
        return count;
    }

    int32_t fail(const char* format, ...)
    {
        if (failed) return None;
        failed = true;
        if (error && errorSize) {
            va_list arguments;
            va_start(arguments, format);
            std::vsnprintf(error, errorSize, format, arguments);
            va_end(arguments);
        }
        return None;
    }

    int32_t add(Kind kind, uint32_t begin)
    {
        if (out.count >= out.capacity) return fail("the formula is too complex to set");
        auto index = static_cast<int32_t>(out.count++);
        auto& node = out.nodes[index];
        node = Node{};
        node.kind = kind;
        node.begin = begin;
        node.end = pos;
        return index;
    }

    Node& at(int32_t index) { return out.nodes[index]; }

    void blank()
    {
        while (pos < size) {
            auto c = source[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos++;
            } else if (c == '%') {
                while (pos < size && source[pos] != '\n') pos++;
            } else {
                break;
            }
        }
    }

    static bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

    // The name of the command at pos, a backslash, without moving: its letters, or the one
    // character after the backslash.
    uint32_t name(uint32_t at, const char*& name) const
    {
        name = source + at + 1u;
        if (at + 1u >= size) return 0u;
        if (!letter(source[at + 1u])) return 1u;
        auto end = at + 1u;
        while (end < size && letter(source[end])) end++;
        return end - at - 1u;
    }

    bool command(const char* word)
    {
        if (pos >= size || source[pos] != '\\') return false;
        const char* text;
        auto length = name(pos, text);
        return std::strlen(word) == length && std::memcmp(text, word, length) == 0;
    }

    // Appends node, and the nodes linked after it, to the list from first to last.
    void link(int32_t& first, int32_t& last, int32_t node)
    {
        if (node == None) return;
        if (last == None) first = node;
        else at(last).next = node;
        last = node;
        while (at(last).next != None) last = at(last).next;
    }

    int32_t list(Stop stop)
    {
        if (++depth > DepthLimit) return fail("the formula nests too deeply at %u", character(pos));
        int32_t first = None;
        int32_t last = None;
        auto opened = pos ? pos - 1u : 0u;
        while (!failed) {
            blank();
            if (pos >= size) {
                if (stop == Stop::Brace) return fail("missing '}' for the '{' at %u", character(opened));
                if (stop == Stop::Bracket) return fail("missing ']' for the '[' at %u", character(opened));
                ended = Ended::Source;
                break;
            }
            auto c = source[pos];
            if (c == '}') {
                if (stop != Stop::Brace) return fail("unexpected '}' at %u", character(pos));
                pos++;
                break;
            }
            if (c == ']' && stop == Stop::Bracket) {
                pos++;
                break;
            }
            if (c == '&') {
                if (stop != Stop::Cell) return fail("'&' outside an environment at %u", character(pos));
                pos++;
                ended = Ended::Ampersand;
                break;
            }
            if (c == '^' || c == '_' || c == '\'') {
                // Scripts with nothing before them to attach to take an empty nucleus.
                if (last == None || at(last).kind == Kind::Space || at(last).kind == Kind::Style) {
                    link(first, last, add(Kind::Group, pos));
                    if (failed) break;
                }
                if (c == '\'') primes(last);
                else script(last, c == '^');
                continue;
            }
            if (c == '\\') {
                const char* word;
                auto length = name(pos, word);
                if (length == 1u && *word == '\\') {
                    if (stop != Stop::Cell) return fail("'\\\\' outside an environment at %u", character(pos));
                    pos += 2u;
                    ended = Ended::Row;
                    break;
                }
                if (length == 5u && std::memcmp(word, "right", 5u) == 0) {
                    if (stop != Stop::Right) return fail("'\\right' at %u has no '\\left'", character(pos));
                    break;
                }
                if (length == 3u && std::memcmp(word, "end", 3u) == 0) {
                    if (stop != Stop::Cell) return fail("'\\end' at %u has no '\\begin'", character(pos));
                    ended = Ended::Environment;
                    break;
                }
                if ((length == 6u && std::memcmp(word, "limits", 6u) == 0)
                    || (length == 8u && std::memcmp(word, "nolimits", 8u) == 0)) {
                    if (last == None || at(last).cls != Class::Op) {
                        return fail("'\\%.*s' at %u does not follow an operator", static_cast<int>(length), word,
                                    character(pos));
                    }
                    at(last).flags = static_cast<uint8_t>((at(last).flags & ~FlagLimits) | FlagLimitsSet
                                                          | (length == 6u ? FlagLimits : 0u));
                    pos += length + 1u;
                    continue;
                }
            }
            link(first, last, atom());
        }
        depth--;
        if (stop == Stop::Right && !failed && pos >= size) return None;
        return failed ? None : first;
    }

    // The list one argument holds: a braced list, or one atom. A missing one fails with
    // message, which names command and where it began.
    int32_t argument(const char* command, uint32_t begin, const char* message = "'\\%s' needs an argument at %u")
    {
        blank();
        if (pos >= size || source[pos] == '}' || source[pos] == '&' || source[pos] == '^' || source[pos] == '_'
            || this->command("right") || this->command("end")) {
            return fail(message, command, character(begin));
        }
        if (source[pos] != '{') return atom();
        pos++;
        return list(Stop::Brace);
    }

    void script(int32_t target, bool up)
    {
        auto begin = pos++;
        auto& node = at(target);
        auto& slot = up ? node.sup : node.sub;
        auto primed = up && (node.flags & FlagPrimed);
        if (slot != None && !primed) {
            fail(up ? "double superscript at %u" : "double subscript at %u", character(begin));
            return;
        }
        auto list = argument(up ? "^" : "_", begin,
                             up ? "'%s' needs something to raise at %u" : "'%s' needs something to lower at %u");
        if (failed) return;
        if (primed) {
            auto last = slot;
            while (at(last).next != None) last = at(last).next;
            at(last).next = list;
            node.flags = static_cast<uint8_t>(node.flags & ~FlagPrimed);
        } else {
            slot = list;
        }
    }

    void primes(int32_t target)
    {
        if (at(target).sup != None && !(at(target).flags & FlagPrimed)) {
            fail("double superscript at %u", character(pos));
            return;
        }
        int32_t first = at(target).sup;
        int32_t last = first;
        while (last != None && at(last).next != None) last = at(last).next;
        while (pos < size && source[pos] == '\'') {
            auto prime = add(Kind::Glyph, pos);
            if (failed) return;
            pos++;
            at(prime).text = "′";
            at(prime).length = static_cast<uint32_t>(std::strlen("′"));
            at(prime).end = pos;
            link(first, last, prime);
        }
        at(target).sup = first;
        at(target).flags = static_cast<uint8_t>(at(target).flags | FlagPrimed);
    }

    // The class and alphabet of a character typed as it is.
    void character(Node& node, uint32_t codepoint)
    {
        node.cls = Class::Ord;
        node.variant = Variant::Upright;
        if ((codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z')) {
            node.variant = variant;
            return;
        }
        if (codepoint >= '0' && codepoint <= '9') {
            if (fixed && (variant == Variant::Bold || variant == Variant::DoubleStruck)) node.variant = variant;
            return;
        }
        switch (codepoint) {
            case '+': node.cls = Class::Bin; return;
            case '-': node.cls = Class::Bin; node.text = "−"; node.length = 3u; return;
            case '*': node.cls = Class::Bin; node.text = "∗"; node.length = 3u; return;
            case '=': case '<': case '>': case ':': node.cls = Class::Rel; return;
            case ',': case ';': node.cls = Class::Punct; return;
            case '(': case '[': node.cls = Class::Open; return;
            case ')': case ']': case '!': case '?': node.cls = Class::Close; return;
            default: break;
        }
        // A symbol typed as itself takes the class its command gives it.
        for (auto& symbol : SYMBOLS) {
            if (node.length == std::strlen(symbol.text) && std::memcmp(node.text, symbol.text, node.length) == 0) {
                node.cls = symbol.cls;
                node.variant = symbol.variant == Variant::Italic ? variant : symbol.variant;
                return;
            }
        }
    }

    int32_t atom()
    {
        if (failed) return None;
        if (++depth > DepthLimit) return fail("the formula nests too deeply at %u", character(pos));
        auto result = atomBody();
        depth--;
        return result;
    }

    int32_t atomBody()
    {
        auto begin = pos;
        auto c = source[pos];
        if (c == '{') {
            pos++;
            auto node = add(Kind::Group, begin);
            if (failed) return None;
            auto body = list(Stop::Brace);
            if (failed) return None;
            at(node).body = body;
            at(node).end = pos;
            return node;
        }
        if (c == '\\') return controlSequence();
        if (c == '~') {
            pos++;
            auto node = add(Kind::Space, begin);
            if (failed) return None;
            at(node).value = 6.0f;
            at(node).flags = FlagMu;
            return node;
        }
        if (c == '$' || c == '#' || c == '^' || c == '_' || c == '&' || c == '}') {
            return fail("unexpected '%c' at %u", c, character(pos));
        }
        uint32_t codepoint;
        auto length = decode(source + pos, size - pos, codepoint);
        if (codepoint == 0xfffdu && length <= 1u && static_cast<uint8_t>(c) >= 0x80u) {
            return fail("the character at %u is not valid UTF-8", character(pos));
        }
        if (codepoint < 0x20u) return fail("unexpected control character at %u", character(pos));
        pos += length;
        auto node = add(Kind::Glyph, begin);
        if (failed) return None;
        at(node).text = source + begin;
        at(node).length = length;
        character(at(node), codepoint);
        return node;
    }

    // The raw text of a text argument, with TeX's escapes undone.
    const char* text(const char* command, uint32_t begin, uint32_t& length)
    {
        blank();
        if (pos >= size || source[pos] != '{') {
            fail("'\\%s' takes its text in braces at %u", command, character(begin));
            return nullptr;
        }
        auto open = pos++;
        auto start = out.stringCount;
        auto nesting = 0u;
        while (pos < size) {
            auto c = source[pos];
            if (c == '}' && !nesting) break;
            if (c == '{') nesting++;
            if (c == '}') nesting--;
            if (c == '{' || c == '}') {
                pos++;
                continue;
            }
            if (c == '\\' && pos + 1u < size && std::strchr("{}$%&#_ \\", source[pos + 1u])) {
                pos++;
                c = source[pos] == '\\' ? ' ' : source[pos];
            }
            if (!keepCharacter(c)) return nullptr;
            pos++;
        }
        if (pos >= size) {
            fail("missing '}' for the '{' at %u", character(open));
            return nullptr;
        }
        pos++;
        length = out.stringCount - start;
        if (!keepCharacter('\0')) return nullptr;
        return out.strings + start;
    }

    bool keepCharacter(char c)
    {
        if (out.stringCount + 1u > out.stringCapacity) {
            fail("the formula is too complex to set");
            return false;
        }
        out.strings[out.stringCount++] = c;
        return true;
    }

    Delim delimiter(const char* command, uint32_t begin)
    {
        blank();
        if (pos >= size) {
            fail("'\\%s' takes a delimiter at %u", command, character(begin));
            return Delim::None;
        }
        uint32_t length = 1u;
        if (source[pos] == '\\') {
            const char* word;
            length = name(pos, word) + 1u;
        }
        auto found = _find(DELIMITERS, source + pos, length);
        if (!found) {
            fail("'\\%s' takes a delimiter at %u, such as ( or \\{", command, character(begin));
            return Delim::None;
        }
        pos += length;
        return found->delim;
    }

    int32_t controlSequence()
    {
        auto begin = pos;
        const char* word;
        auto length = name(pos, word);
        if (!length) return fail("a lone '\\' at %u", character(pos));
        pos += length + 1u;

        if (auto symbol = _find(SYMBOLS, word, length)) {
            auto node = add(Kind::Glyph, begin);
            if (failed) return None;
            at(node).text = symbol->text;
            at(node).length = static_cast<uint32_t>(std::strlen(symbol->text));
            at(node).cls = symbol->cls;
            at(node).variant = symbol->variant;
            if (fixed && symbol->variant == Variant::Italic) at(node).variant = variant;
            if (fixed && variant == Variant::Bold) at(node).variant = Variant::Bold;
            return node;
        }
        if (auto op = _find(OPERATORS, word, length)) {
            auto large = (op->flags & FlagLarge) != 0u;
            auto node = add(large ? Kind::Glyph : Kind::Word, begin);
            if (failed) return None;
            at(node).text = op->text;
            at(node).length = static_cast<uint32_t>(std::strlen(op->text));
            at(node).cls = Class::Op;
            at(node).flags = op->flags;
            return node;
        }
        if (auto space = _find(SPACES, word, length)) {
            auto node = add(Kind::Space, begin);
            if (failed) return None;
            at(node).value = space->mu;
            at(node).flags = FlagMu;
            return node;
        }
        if (auto accent = _find(ACCENTS, word, length)) {
            auto node = add(Kind::Accent, begin);
            if (failed) return None;
            at(node).left = accent->value;
            auto body = argument(accent->name, begin);
            if (failed) return None;
            at(node).body = body;
            return node;
        }
        if (auto font = _find(FONTS, word, length)) {
            auto node = add(Kind::Group, begin);
            if (failed) return None;
            auto saved = variant;
            auto savedFixed = fixed;
            variant = static_cast<Variant>(font->value);
            fixed = true;
            auto body = argument(font->name, begin);
            variant = saved;
            fixed = savedFixed;
            if (failed) return None;
            at(node).body = body;
            return node;
        }
        if (auto text = _find(TEXTS, word, length)) {
            uint32_t textLength = 0u;
            auto value = this->text(text->name, begin, textLength);
            if (failed) return None;
            auto node = add(Kind::Word, begin);
            if (failed) return None;
            at(node).text = value;
            at(node).length = textLength;
            at(node).variant = static_cast<Variant>(text->value);
            return node;
        }
        if (auto style = _find(STYLES, word, length)) {
            auto node = add(Kind::Style, begin);
            if (failed) return None;
            at(node).style = style->value;
            return node;
        }
        auto is = [word, length](const char* name) {
            return std::strlen(name) == length && std::memcmp(word, name, length) == 0;
        };
        if (is("frac") || is("dfrac") || is("tfrac") || is("binom") || is("dbinom") || is("tbinom")) {
            char label[8] = {};
            std::memcpy(label, word, length);
            auto node = add(Kind::Fraction, begin);
            if (failed) return None;
            if (word[0] == 'd') at(node).style = static_cast<uint8_t>(Level::Display) + 1u;
            if (word[0] == 't') at(node).style = static_cast<uint8_t>(Level::Text) + 1u;
            if (word[length - 1u] == 'm') at(node).flags = FlagNoBar;
            auto numerator = argument(label, begin);
            if (failed) return None;
            at(node).body = numerator;
            auto denominator = argument(label, begin);
            if (failed) return None;
            at(node).extra = denominator;
            return node;
        }
        if (is("sqrt")) {
            auto node = add(Kind::Root, begin);
            if (failed) return None;
            blank();
            if (pos < size && source[pos] == '[') {
                pos++;
                auto index = list(Stop::Bracket);
                if (failed) return None;
                at(node).extra = index;
            }
            auto body = argument("sqrt", begin);
            if (failed) return None;
            at(node).body = body;
            return node;
        }
        if (is("left")) return fence(begin);
        if (is("middle")) return fail("'\\middle' at %u is not supported; split the fence in two", character(begin));
        if (is("big") || is("Big") || is("bigg") || is("Bigg") || is("bigl") || is("Bigl") || is("biggl")
            || is("Biggl") || is("bigr") || is("Bigr") || is("biggr") || is("Biggr") || is("bigm") || is("Bigm")
            || is("biggm") || is("Biggm")) {
            char label[8] = {};
            std::memcpy(label, word, length);
            auto twice = length >= 4u && word[3] == 'g';
            auto tail = length > (twice ? 4u : 3u) ? word[twice ? 4 : 3] : '\0';
            auto delim = delimiter(label, begin);
            if (failed) return None;
            auto node = add(Kind::Delimiter, begin);
            if (failed) return None;
            at(node).left = static_cast<uint8_t>(delim);
            at(node).value = (word[0] == 'B' ? 1.8f : 1.2f) + (twice ? 1.2f : 0.0f);
            at(node).cls = tail == 'l' ? Class::Open : tail == 'r' ? Class::Close : tail == 'm' ? Class::Rel : Class::Ord;
            return node;
        }
        if (is("langle") || is("rangle") || is("lfloor") || is("rfloor") || is("lceil") || is("rceil")) {
            // Fonts seldom have these, so they are drawn as delimiters of the normal size.
            pos = begin;
            auto delim = delimiter("left", begin);
            if (failed) return None;
            auto node = add(Kind::Delimiter, begin);
            if (failed) return None;
            at(node).left = static_cast<uint8_t>(delim);
            at(node).value = 0.0f;
            at(node).cls = word[0] == 'l' ? Class::Open : Class::Close;
            return node;
        }
        if (is("operatorname") || is("mathop")) {
            auto limits = pos < size && source[pos] == '*';
            if (limits) pos++;
            uint32_t textLength = 0u;
            auto value = text(is("mathop") ? "mathop" : "operatorname", begin, textLength);
            if (failed) return None;
            auto node = add(Kind::Word, begin);
            if (failed) return None;
            at(node).text = value;
            at(node).length = textLength;
            at(node).cls = Class::Op;
            if (limits) at(node).flags = FlagLimits;
            return node;
        }
        if (is("not")) {
            blank();
            auto start = pos;
            uint32_t relation = 1u;
            if (pos < size && source[pos] == '\\') {
                const char* next;
                relation = name(pos, next) + 1u;
            }
            for (auto& negation : NEGATIONS) {
                if (std::strlen(negation.relation) == relation
                    && std::memcmp(source + start, negation.relation, relation) == 0) {
                    pos = start + relation;
                    auto node = add(Kind::Glyph, begin);
                    if (failed) return None;
                    at(node).text = negation.text;
                    at(node).length = static_cast<uint32_t>(std::strlen(negation.text));
                    at(node).cls = Class::Rel;
                    return node;
                }
            }
            return fail("'\\not' at %u negates only =, <, >, \\in, \\equiv, \\sim, \\approx, \\subset, \\supset, "
                        "\\le and \\ge", character(begin));
        }
        if (is("begin")) return environment(begin);
        if (is("end")) return fail("'\\end' at %u has no '\\begin'", character(begin));
        if (is("right")) return fail("'\\right' at %u has no '\\left'", character(begin));
        if (is("\\")) return fail("'\\\\' outside an environment at %u", character(begin));
        if (is("limits") || is("nolimits")) {
            return fail("'\\%.*s' at %u does not follow an operator", static_cast<int>(length), word, character(begin));
        }
        return fail("unknown command '\\%.*s' at %u", static_cast<int>(length), word, character(begin));
    }

    int32_t fence(uint32_t begin)
    {
        auto left = delimiter("left", begin);
        if (failed) return None;
        auto node = add(Kind::Fence, begin);
        if (failed) return None;
        at(node).left = static_cast<uint8_t>(left);
        at(node).cls = Class::Inner;
        auto body = list(Stop::Right);
        if (failed) return None;
        if (!command("right")) return fail("'\\left' at %u has no '\\right'", character(begin));
        auto close = pos;
        at(node).close = close;
        pos += 6u;
        auto right = delimiter("right", close);
        if (failed) return None;
        at(node).body = body;
        at(node).right = static_cast<uint8_t>(right);
        at(node).closeEnd = pos;
        return node;
    }

    // The name of an environment in braces after \begin or \end.
    bool environmentName(uint32_t begin, const char*& word, uint32_t& length)
    {
        blank();
        if (pos >= size || source[pos] != '{') {
            fail("'\\begin' and '\\end' take the name of an environment in braces at %u", character(begin));
            return false;
        }
        auto start = ++pos;
        while (pos < size && (letter(source[pos]) || source[pos] == '*')) pos++;
        if (pos >= size || source[pos] != '}') {
            fail("'\\begin' and '\\end' take the name of an environment in braces at %u", character(begin));
            return false;
        }
        word = source + start;
        length = pos - start;
        pos++;
        return true;
    }

    int32_t environment(uint32_t begin)
    {
        const char* word;
        uint32_t length;
        if (!environmentName(begin, word, length)) return None;
        auto found = _find(ENVIRONMENTS, word, length);
        if (!found) return fail("unknown environment '%.*s' at %u", static_cast<int>(length), word, character(begin));
        auto node = add(Kind::Array, begin);
        if (failed) return None;
        at(node).left = found->value;
        at(node).cls = Class::Inner;
        int32_t firstRow = None;
        int32_t lastRow = None;
        while (!failed) {
            auto row = add(Kind::Row, pos);
            if (failed) return None;
            int32_t firstCell = None;
            int32_t lastCell = None;
            auto done = false;
            while (!failed) {
                auto cell = add(Kind::Cell, pos);
                if (failed) return None;
                auto body = list(Stop::Cell);
                if (failed) return None;
                at(cell).body = body;
                at(cell).end = pos;
                link(firstCell, lastCell, cell);
                if (ended == Ended::Ampersand) continue;
                if (ended == Ended::Source) {
                    return fail("'\\begin{%.*s}' at %u has no '\\end'", static_cast<int>(length), word, character(begin));
                }
                done = ended == Ended::Environment;
                break;
            }
            at(row).body = firstCell;
            at(row).end = pos;
            // A final \\ before \end leaves no row of its own.
            auto empty = firstCell == lastCell && at(firstCell).body == None;
            if (!(done && empty && firstRow != None)) link(firstRow, lastRow, row);
            if (done) break;
        }
        auto close = pos;
        at(node).close = close;
        pos += 4u;
        const char* closing;
        uint32_t closingLength;
        if (!environmentName(close, closing, closingLength)) return None;
        if (closingLength != length || std::memcmp(closing, word, length) != 0) {
            return fail("'\\end{%.*s}' at %u closes '\\begin{%.*s}'", static_cast<int>(closingLength), closing,
                        character(close), static_cast<int>(length), word);
        }
        at(node).body = firstRow;
        at(node).end = at(node).closeEnd = pos;
        return node;
    }
};

}  // namespace

bool parse(const char* source, uint32_t size, Parse& output, char* error, uint32_t errorSize) noexcept
{
    if (error && errorSize) error[0] = '\0';
    delete[] output.nodes;
    delete[] output.strings;
    output.nodes = nullptr;
    output.strings = nullptr;
    output.count = output.stringCount = 0u;
    output.first = None;
    if (!source || size > SourceLimit) {
        if (error && errorSize) std::snprintf(error, errorSize, "a formula holds at most %u bytes", SourceLimit);
        return false;
    }
    output.capacity = 2u * size + 64u;
    output.stringCapacity = size + 64u;
    output.nodes = new (std::nothrow) Node[output.capacity];
    output.strings = new (std::nothrow) char[output.stringCapacity];
    if (!output.nodes || !output.strings) {
        if (error && errorSize) std::snprintf(error, errorSize, "out of memory");
        return false;
    }
    Parser parser(source, size, output, error, errorSize);
    output.first = parser.list(Stop::End);
    return !parser.failed;
}

}  // namespace vis::tex::detail
