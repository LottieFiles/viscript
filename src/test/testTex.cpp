#include <cmath>
#include <cstdio>
#include <cstring>

#include "vis_tex.h"
#include "visTex.h"

using namespace vis;
using namespace vis::tex::detail;

static int failures = 0;

static void check(bool condition, const char* expression, int line)
{
    if (condition) return;
    std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, line, expression);
    failures++;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static bool _parse(const char* source, Parse& parse)
{
    char error[256] = {};
    auto parsed = tex::detail::parse(source, static_cast<uint32_t>(std::strlen(source)), parse, error, sizeof(error));
    if (!parsed) std::fprintf(stderr, "%s: %s\n", source, error);
    return parsed;
}

static bool _text(const Node& node, const char* text)
{
    return node.length == std::strlen(text) && std::memcmp(node.text, text, node.length) == 0;
}

// The nodes of the list from first, in order; returns how many there are.
static uint32_t _list(const Parse& parse, int32_t first, const Node** nodes, uint32_t capacity)
{
    auto count = 0u;
    for (auto index = first; index != None; index = parse.nodes[index].next) {
        if (count < capacity) nodes[count] = parse.nodes + index;
        count++;
    }
    return count;
}

static void _error(const char* source, const char* expected)
{
    char error[256] = {};
    CHECK(tex::check(source, error, sizeof(error)) == Result::InvalidArguments);
    if (!std::strstr(error, expected)) {
        std::fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", source, expected, error);
        failures++;
    }
}

static void _atoms()
{
    Parse parse;
    CHECK(_parse("a^2+b^2=c^2", parse));
    const Node* nodes[8] = {};
    CHECK(_list(parse, parse.first, nodes, 8) == 5u);
    CHECK(_text(*nodes[0], "a") && nodes[0]->cls == Class::Ord && nodes[0]->variant == Variant::Italic);
    CHECK(nodes[0]->begin == 0u && nodes[0]->end == 1u && nodes[0]->spanEnd == 3u);
    CHECK(nodes[0]->sup != None && _text(parse.nodes[nodes[0]->sup], "2"));
    CHECK(parse.nodes[nodes[0]->sup].variant == Variant::Upright && nodes[0]->sub == None);
    CHECK(_text(*nodes[1], "+") && nodes[1]->cls == Class::Bin);
    CHECK(_text(*nodes[3], "=") && nodes[3]->cls == Class::Rel);
    CHECK(_text(*nodes[4], "c") && nodes[4]->begin == 8u && nodes[4]->spanEnd == 11u);

    // A minus is the minus sign, and a lone script takes an empty nucleus.
    CHECK(_parse("-x_{i j}^{\\prime}", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 2u);
    CHECK(_text(*nodes[0], "−") && nodes[0]->cls == Class::Bin);
    CHECK(_list(parse, nodes[1]->sub, nodes, 8) == 2u && _text(*nodes[1], "j"));
    CHECK(_parse("^{14}C", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 2u);
    CHECK(nodes[0]->kind == Kind::Group && nodes[0]->body == None && nodes[0]->sup != None);

    // Primes raise as a superscript that a ^ may extend.
    CHECK(_parse("f''", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 1u);
    CHECK(_list(parse, nodes[0]->sup, nodes, 8) == 2u && _text(*nodes[0], "′"));
    CHECK(_parse("f'^2", parse));
    CHECK(_list(parse, parse.nodes[parse.first].sup, nodes, 8) == 2u && _text(*nodes[1], "2"));

    // Commands name symbols in their classes and alphabets.
    CHECK(_parse("\\alpha\\Gamma\\le\\times\\infty\\cdots", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 6u);
    CHECK(_text(*nodes[0], "α") && nodes[0]->variant == Variant::Italic);
    CHECK(_text(*nodes[1], "Γ") && nodes[1]->variant == Variant::Upright);
    CHECK(nodes[2]->cls == Class::Rel && nodes[3]->cls == Class::Bin && nodes[4]->cls == Class::Ord);
    CHECK(nodes[5]->cls == Class::Inner);
    // A symbol typed as itself takes its class too.
    CHECK(_parse("a≤b", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 3u && nodes[1]->cls == Class::Rel);
}

static void _structures()
{
    Parse parse;
    const Node* nodes[8] = {};
    CHECK(_parse("\\frac{a+b}{2} \\dfrac12 \\binom{n}{k}", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 3u);
    CHECK(nodes[0]->kind == Kind::Fraction && nodes[0]->style == 0u && !(nodes[0]->flags & FlagNoBar));
    const Node* parts[4] = {};
    CHECK(_list(parse, nodes[0]->body, parts, 4) == 3u && _list(parse, nodes[0]->extra, parts, 4) == 1u);
    CHECK(nodes[1]->style == static_cast<uint8_t>(Level::Display) + 1u);
    CHECK(_text(parse.nodes[nodes[1]->body], "1") && _text(parse.nodes[nodes[1]->extra], "2"));
    CHECK(nodes[2]->flags & FlagNoBar);

    CHECK(_parse("\\sqrt[3]{x}\\sqrt y", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 2u);
    CHECK(nodes[0]->kind == Kind::Root && nodes[0]->extra != None && _text(parse.nodes[nodes[0]->extra], "3"));
    CHECK(nodes[1]->extra == None && _text(parse.nodes[nodes[1]->body], "y"));

    CHECK(_parse("\\left( \\frac{1}{x} \\right] \\left. x \\right\\} \\left\\langle v \\right\\rangle", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 3u);
    CHECK(nodes[0]->kind == Kind::Fence && nodes[0]->cls == Class::Inner);
    CHECK(nodes[0]->left == static_cast<uint8_t>(Delim::LeftParen));
    CHECK(nodes[0]->right == static_cast<uint8_t>(Delim::RightBracket));
    CHECK(nodes[1]->left == static_cast<uint8_t>(Delim::None));
    CHECK(nodes[1]->right == static_cast<uint8_t>(Delim::RightBrace));
    CHECK(nodes[2]->left == static_cast<uint8_t>(Delim::LeftAngle));

    CHECK(_parse("\\sum_{i=1}^{n} \\int\\limits_0^1 \\lim_{x\\to 0} \\sin x", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 5u);
    CHECK(nodes[0]->kind == Kind::Glyph && nodes[0]->cls == Class::Op);
    CHECK((nodes[0]->flags & FlagLarge) && (nodes[0]->flags & FlagLimits));
    CHECK((nodes[1]->flags & FlagIntegral) && (nodes[1]->flags & FlagLimits) && (nodes[1]->flags & FlagLimitsSet));
    CHECK(nodes[2]->kind == Kind::Word && _text(*nodes[2], "lim") && (nodes[2]->flags & FlagLimits));
    CHECK(nodes[3]->kind == Kind::Word && !(nodes[3]->flags & FlagLimits));

    CHECK(_parse("\\hat{x} \\vec v \\overline{AB} \\mathbb{R} \\mathrm{d\\alpha} \\text{if } \\, \\quad", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 8u);
    CHECK(nodes[0]->kind == Kind::Accent && nodes[0]->left == static_cast<uint8_t>(Accent::Hat));
    CHECK(nodes[1]->left == static_cast<uint8_t>(Accent::Vec) && _text(parse.nodes[nodes[1]->body], "v"));
    CHECK(nodes[2]->left == static_cast<uint8_t>(Accent::Overline));
    CHECK(nodes[3]->kind == Kind::Group && parse.nodes[nodes[3]->body].variant == Variant::DoubleStruck);
    CHECK(_list(parse, nodes[4]->body, parts, 4) == 2u);
    CHECK(parts[0]->variant == Variant::Upright && parts[1]->variant == Variant::Upright);
    CHECK(nodes[5]->kind == Kind::Word && _text(*nodes[5], "if "));
    CHECK(nodes[6]->kind == Kind::Space && nodes[6]->value == 3.0f && (nodes[6]->flags & FlagMu));
    CHECK(nodes[7]->value == 18.0f);

    CHECK(_parse("\\begin{pmatrix} a & b \\\\ c & d \\\\ \\end{pmatrix}", parse));
    CHECK(_list(parse, parse.first, nodes, 8) == 1u);
    CHECK(nodes[0]->kind == Kind::Array && nodes[0]->left == static_cast<uint8_t>(Environment::PMatrix));
    const Node* rows[4] = {};
    CHECK(_list(parse, nodes[0]->body, rows, 4) == 2u);
    CHECK(_list(parse, rows[1]->body, parts, 4) == 2u && _text(parse.nodes[parts[1]->body], "d"));
    CHECK(_parse("x = \\begin{cases} 1 & x > 0 \\\\ \\begin{bmatrix} a \\end{bmatrix} & \\text{else} \\end{cases}",
                 parse));
}

static void _errors()
{
    _error("a+\\foo", "unknown command '\\foo' at 3");
    _error("α+\\foo", "unknown command '\\foo' at 3");
    _error("\\frac{a}", "'\\frac' needs an argument at 1");
    _error("{a", "missing '}' for the '{' at 1");
    _error("a}", "unexpected '}' at 2");
    _error("x^2^3", "double superscript at 4");
    _error("x_1_2", "double subscript at 4");
    _error("x^", "'^' needs something to raise at 2");
    _error("\\left( x", "'\\left' at 1 has no '\\right'");
    _error("x \\right)", "'\\right' at 3 has no '\\left'");
    _error("\\left x \\right)", "'\\left' takes a delimiter at 1");
    _error("a & b", "'&' outside an environment at 3");
    _error("a \\\\ b", "'\\\\' outside an environment at 3");
    _error("\\begin{foo} a \\end{foo}", "unknown environment 'foo' at 1");
    _error("\\begin{pmatrix} a \\end{bmatrix}", "'\\end{bmatrix}' at 19 closes '\\begin{pmatrix}'");
    _error("\\begin{cases} a", "'\\begin{cases}' at 1 has no '\\end'");
    _error("\\end{cases}", "'\\end' at 1 has no '\\begin'");
    _error("x\\limits", "'\\limits' at 2 does not follow an operator");
    _error("\\not\\pi", "'\\not' at 1 negates only");
    _error("a $ b", "unexpected '$' at 3");
    _error("\\text x", "'\\text' takes its text in braces at 1");
    char deep[200] = {};
    for (auto i = 0u; i < 70u; i++) deep[i] = '{';
    _error(deep, "the formula nests too deeply");
    CHECK(tex::check("\\frac{\\alpha}{2} \\le \\sqrt{x^2 + 1}") == Result::Success);
}

static void _alphabets()
{
    CHECK(alphabet('h', Variant::Italic) == 0x210eu);
    CHECK(alphabet('a', Variant::Italic) == 0x1d44eu);
    CHECK(alphabet(0x3b1u, Variant::Italic) == 0x1d6fcu);
    CHECK(alphabet('R', Variant::DoubleStruck) == 0x211du);
    CHECK(alphabet('A', Variant::DoubleStruck) == 0x1d538u);
    CHECK(alphabet('a', Variant::Bold) == 0x1d41au && alphabet('7', Variant::Bold) == 0x1d7d5u);
    CHECK(alphabet('L', Variant::Script) == 0x2112u && alphabet('x', Variant::Script) == 0x1d4cdu);
    CHECK(alphabet('+', Variant::Bold) == '+' && alphabet('a', Variant::Upright) == 'a');
    char utf8[4];
    uint32_t codepoint = 0;
    CHECK(encode(0x1d44eu, utf8) == 4u && decode(utf8, 4u, codepoint) == 4u && codepoint == 0x1d44eu);
}

// A font whose numbers keep TeX's rules easy to follow: glyphs half an em wide and 0.7 tall,
// the x-height and axis of Computer Modern, and no slant.
struct FakeMetrics : Metrics
{
    bool glyph(const char* text, uint32_t length, Variant variant, Glyph& output) noexcept override
    {
        output = Glyph{};
        auto count = 0u;
        for (auto i = 0u; i < length; i++) {
            if ((static_cast<uint8_t>(text[i]) & 0xc0u) != 0x80u) count++;
        }
        output.advance = output.right = 0.5f * static_cast<float>(count);
        output.top = 0.7f;
        if (count != 1u) return true;
        uint32_t c = 0;
        decode(text, length, c);
        if (c == 0x2603u) return false;
        if (variant == Variant::DoubleStruck) {
            output.advance = output.right = 0.0f;
            output.top = 0.0f;
            output.missing = true;
            return true;
        }
        switch (c) {
            case 'a': case 'c': case 'e': case 'm': case 'n': case 'o': case 'r': case 's': case 'u': case 'v':
            case 'w': case 'x': case 'z': output.top = 0.431f; break;
            case 'g': case 'p': case 'q': case 'y': output.top = 0.431f; output.bottom = -0.2f; break;
            case 'f': output.right = 0.6f; break;
            case '+': output.top = 0.58f; output.bottom = -0.08f; break;
            case 0x2212u: output.top = 0.27f; output.bottom = 0.23f; break;
            case '=': output.top = 0.37f; output.bottom = 0.13f; break;
            case '(': case ')': case '[': case ']': case '{': case '}': case '|':
                output.top = 0.75f;
                output.bottom = -0.25f;
                break;
            case 0x2211u:
                output.advance = output.right = 1.0f;
                output.top = 0.75f;
                output.bottom = -0.25f;
                break;
            case 0x222bu:
                output.advance = 0.4f;
                output.right = 0.6f;
                output.top = 0.8f;
                output.bottom = -0.3f;
                break;
            case 0x2032u:
                output.advance = output.right = 0.3f;
                output.top = 0.75f;
                output.bottom = 0.45f;
                break;
            default: break;
        }
        return true;
    }
};

static bool _near(float a, float b)
{
    return std::fabs(a - b) < 1e-4f;
}

static bool _set(const char* source, bool display, Layout& layout)
{
    Parse parse;
    if (!_parse(source, parse)) return false;
    FakeMetrics metrics;
    char error[256] = {};
    auto set = tex::detail::layout(parse, display, metrics, layout, error, sizeof(error));
    if (!set) std::fprintf(stderr, "%s: %s\n", source, error);
    return set;
}

// The nth text item that shows text, or the nth item of kind when text is null.
static const Item* _item(const Layout& layout, const char* text, ItemKind kind = ItemKind::Text, uint32_t nth = 0)
{
    for (auto i = 0u; i < layout.count; i++) {
        auto& item = layout.items[i];
        if (item.kind != kind) continue;
        if (text && (item.length != std::strlen(text) || std::memcmp(item.text, text, item.length) != 0)) continue;
        if (nth-- == 0u) return &item;
    }
    static const Item none = {};
    std::fprintf(stderr, "no item %s\n", text ? text : "of that kind");
    failures++;
    return &none;
}

static void _fractions()
{
    Layout layout;
    // The bar stands on the axis, the parts at TeX's baselines for text style, a size down.
    CHECK(_set("\\frac{a}{b}", false, layout));
    auto bar = _item(layout, nullptr, ItemKind::Rule);
    CHECK(_near(bar->y + bar->height * 0.5f, 0.25f) && _near(bar->height, 0.04f));
    CHECK(_near(bar->x, 0.12f) && _near(bar->width, 0.35f) && _near(layout.width, 0.59f));
    CHECK(_near(_item(layout, "a")->y, 0.394f) && _near(_item(layout, "a")->scale, 0.7f));
    CHECK(_near(_item(layout, "b")->y, -0.345f) && _near(_item(layout, "b")->x, 0.12f));
    // Items keep the order of the source: \frac, then its numerator and denominator.
    CHECK(layout.count == 3u && layout.items[0].kind == ItemKind::Rule && layout.items[0].begin == 0u);
    CHECK(layout.items[0].end == 5u && layout.items[1].begin == 6u);
    // In display style the parts stay their size and move out.
    CHECK(_set("\\frac{a}{b}", true, layout));
    CHECK(_near(_item(layout, "a")->y, 0.677f) && _near(_item(layout, "a")->scale, 1.0f));
    CHECK(_near(_item(layout, "b")->y, -0.686f));
    // A numerator that reaches down keeps its clearance from the bar.
    CHECK(_set("\\frac{g}{x}", false, layout));
    CHECK(_near(_item(layout, "g")->y, 0.27f + 0.04f + 0.14f));
    // \tfrac sets text style inside a display.
    CHECK(_set("\\tfrac{a}{b}", true, layout));
    CHECK(_near(_item(layout, "a")->scale, 0.7f));
    // A binomial has no bar and takes parentheses.
    CHECK(_set("\\binom{n}{k}", false, layout));
    CHECK(_item(layout, nullptr, ItemKind::Delimiter)->shape == static_cast<uint8_t>(Delim::LeftParen));
    CHECK(_item(layout, nullptr, ItemKind::Delimiter, 1)->shape == static_cast<uint8_t>(Delim::RightParen));
    CHECK(layout.items[0].kind == ItemKind::Delimiter && layout.items[3].kind == ItemKind::Delimiter);
}

static void _scripts()
{
    Layout layout;
    CHECK(_set("x^2", false, layout));
    CHECK(_near(_item(layout, "2")->y, 0.363f) && _near(_item(layout, "2")->x, 0.5f));
    CHECK(_near(_item(layout, "2")->scale, 0.7f) && _near(layout.width, 0.5f + 0.35f + 0.05f));
    CHECK(_set("x_i", false, layout));
    CHECK(_near(_item(layout, "i")->y, -0.15f) && _near(_item(layout, "i")->x, 0.5f));
    // Both scripts keep 4 rule thicknesses apart.
    CHECK(_set("x_i^2", false, layout));
    CHECK(_near(_item(layout, "2")->y, 0.363f) && _near(_item(layout, "i")->y, -0.287f));
    CHECK(layout.items[1].begin == 2u && layout.items[2].begin == 4u);
    CHECK(_set("x^2_i", false, layout));
    CHECK(_near(_item(layout, "i")->y, -0.287f) && layout.items[1].begin == 2u);
    // A superscript clears what a glyph's outline reaches past its advance; a subscript
    // tucks under it.
    CHECK(_set("f^2", false, layout));
    CHECK(_near(_item(layout, "2")->x, 0.6f));
    CHECK(_set("f_1^2", false, layout));
    CHECK(_near(_item(layout, "1")->x, 0.5f) && _near(_item(layout, "2")->x, 0.6f));
    // Primes stand at the size of the nucleus, on its baseline.
    CHECK(_set("f'", false, layout));
    CHECK(_near(_item(layout, "′")->scale, 1.0f) && _near(_item(layout, "′")->y, 0.0f));
    CHECK(_near(_item(layout, "′")->x, 0.6f));
    CHECK(_set("f'^2", false, layout));
    CHECK(_near(_item(layout, "2")->x, 0.9f) && _near(_item(layout, "2")->y, 0.363f));
    // A superscript in display style rises higher; a cramped one less.
    CHECK(_set("x^2", true, layout));
    CHECK(_near(_item(layout, "2")->y, 0.413f));
    CHECK(_set("\\sqrt{x^2}", false, layout));
    CHECK(_near(_item(layout, "2")->y, 0.289f));
}

static void _operators()
{
    Layout layout;
    // In display style a sum grows, centers on the axis and takes limits above and below.
    CHECK(_set("\\sum_{i=1}^{n}", true, layout));
    auto sum = _item(layout, "∑");
    CHECK(_near(sum->scale, 1.35f) && _near(sum->y, -0.0875f));
    CHECK(_near(_item(layout, "n")->y, 1.125f) && _near(_item(layout, "n")->x, 0.5f));
    CHECK(_near(_item(layout, "i")->y, -1.081f) && _near(_item(layout, "i")->x, 0.15f));
    // The relation in a limit takes no thick spaces in script style.
    CHECK(_near(_item(layout, "=")->x - _item(layout, "i")->x, 0.35f));
    // In text style the limits stand beside it as scripts.
    CHECK(_set("\\sum_{i=1}^{n}", false, layout));
    CHECK(_near(_item(layout, "∑")->scale, 1.0f) && _near(_item(layout, "∑")->y, 0.0f));
    CHECK(_near(_item(layout, "n")->x, 1.0f) && _item(layout, "i")->y < 0.0f);
    // \limits puts them above and below even so.
    CHECK(_set("\\sum\\limits_{i}", false, layout));
    CHECK(_item(layout, "i")->y < -0.5f);
    // An integral's superscript stands right of its subscript.
    CHECK(_set("\\int_0^1", true, layout));
    CHECK(_item(layout, "1")->x > _item(layout, "0")->x + 0.3f);
    CHECK(_item(layout, "∫")->scale > 1.5f);
    // A named operator with limits takes them below in display style only.
    CHECK(_set("\\lim_{x\\to 0} f", true, layout));
    CHECK(_item(layout, "x")->y < -0.5f && _item(layout, "x")->x < 1.0f);
    CHECK(_set("\\lim_{x\\to 0} f", false, layout));
    CHECK(_item(layout, "x")->x >= 1.5f);
}

static void _radicals()
{
    Layout layout;
    // The bar stands a clearance over the radicand: the rule and a quarter of it in text
    // style, a quarter of the x-height more in display.
    CHECK(_set("\\sqrt{x}", false, layout));
    auto sign = _item(layout, nullptr, ItemKind::Radical);
    auto bar = _item(layout, nullptr, ItemKind::Rule);
    CHECK(_near(bar->y, 0.481f) && _near(bar->x, sign->x + sign->width) && _near(bar->width, 0.5f));
    CHECK(_near(sign->y + sign->height, bar->y + bar->height) && sign->y < 0.0f);
    CHECK(_near(_item(layout, "x")->x, bar->x));
    CHECK(_set("\\sqrt{x}", true, layout));
    CHECK(_near(_item(layout, nullptr, ItemKind::Rule)->y, 0.431f + 0.04f + 0.10775f));
    // An index stands small over the sign's left arm, before the radicand in the source.
    CHECK(_set("\\sqrt[3]{x}", false, layout));
    auto index = _item(layout, "3");
    CHECK(_near(index->scale, 0.5f) && index->y > 0.2f && index->x > 0.0f);
    CHECK(layout.items[0].kind == ItemKind::Radical && layout.items[1].kind == ItemKind::Rule);
    CHECK(layout.items[2].text && layout.items[2].text[0] == '3');
}

static void _fences()
{
    Layout layout;
    // A short body takes the font's own parentheses, keyed by \left( and \right).
    CHECK(_set("\\left( x \\right)", false, layout));
    CHECK(layout.count == 3u && _item(layout, "(")->begin == 0u && _item(layout, "(")->end == 6u);
    CHECK(_item(layout, ")")->begin == 9u && _item(layout, ")")->end == 16u);
    // A taller one takes drawn delimiters, centered on the axis and covering it.
    CHECK(_set("\\left[ \\frac{a}{b} \\right]^2", false, layout));
    auto left = _item(layout, nullptr, ItemKind::Delimiter);
    auto right = _item(layout, nullptr, ItemKind::Delimiter, 1);
    CHECK(left->shape == static_cast<uint8_t>(Delim::LeftBracket) && right->shape == static_cast<uint8_t>(Delim::RightBracket));
    CHECK(_near(left->y + left->height * 0.5f, 0.25f) && left->height > 1.0f && _near(right->height, left->height));
    CHECK(right->begin == 19u && right->end == 26u);
    CHECK(_item(layout, "2")->x >= right->x + right->width);
    // A null delimiter draws nothing and takes its space.
    CHECK(_set("\\left. x \\right|", false, layout));
    CHECK(layout.count == 2u && _near(_item(layout, "x")->x, 0.12f));
    // \big sets a fixed size; a bare \langle the normal one.
    CHECK(_set("\\Big( \\langle", false, layout));
    CHECK(_near(_item(layout, nullptr, ItemKind::Delimiter)->height, 1.8f));
    CHECK(_near(_item(layout, nullptr, ItemKind::Delimiter, 1)->height, 1.0f));
}

static void _spacing()
{
    Layout layout;
    CHECK(_set("a+b", false, layout));
    CHECK(_near(_item(layout, "+")->x, 0.5f + 4.0f / 18.0f));
    CHECK(_near(_item(layout, "b")->x, 1.0f + 8.0f / 18.0f));
    CHECK(_set("a=b", false, layout));
    CHECK(_near(_item(layout, "=")->x, 0.5f + 5.0f / 18.0f));
    // A binary operator with nothing to join is ordinary.
    CHECK(_set("-a", false, layout));
    CHECK(_near(_item(layout, "a")->x, 0.5f));
    CHECK(_set("a+=b", false, layout));
    CHECK(_near(_item(layout, "+")->x, 0.5f) && _near(_item(layout, "=")->x, 1.0f + 5.0f / 18.0f));
    // Scripts take no medium or thick spaces.
    CHECK(_set("x^{a+b}", false, layout));
    CHECK(_near(_item(layout, "+")->x - _item(layout, "a")->x, 0.35f));
    CHECK(_set("a,b \\sin x", false, layout));
    CHECK(_near(_item(layout, "b")->x, 1.0f + 3.0f / 18.0f));
    CHECK(_near(_item(layout, "x")->x - _item(layout, "sin")->x, 1.5f + 3.0f / 18.0f));
    // Spaces add to the spacing of the classes.
    CHECK(_set("a\\quad b \\, c", false, layout));
    CHECK(_near(_item(layout, "b")->x, 1.5f) && _near(_item(layout, "c")->x, 2.0f + 3.0f / 18.0f));
}

static void _arrays()
{
    Layout layout;
    // Rows stand on struts, centered on the axis; columns 1em apart.
    CHECK(_set("\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}", false, layout));
    auto left = _item(layout, nullptr, ItemKind::Delimiter);
    CHECK(_near(_item(layout, "a")->y, 0.61f) && _near(_item(layout, "c")->y, -0.59f));
    CHECK(_near(_item(layout, "b")->y, 0.61f) && _near(_item(layout, "b")->x - _item(layout, "a")->x, 1.5f));
    CHECK(_near(left->y + left->height * 0.5f, 0.25f) && _near(left->height, 2.1624f));
    CHECK(left->begin == 0u && left->end == 15u && layout.items[0].kind == ItemKind::Delimiter);
    auto right = _item(layout, nullptr, ItemKind::Delimiter, 1);
    CHECK(right->begin == 31u && right->end == 44u && layout.items[layout.count - 1u].kind == ItemKind::Delimiter);
    // Cells center in their columns; a bare matrix has no delimiters.
    CHECK(_set("\\begin{matrix} aa \\\\ b \\end{matrix}", false, layout));
    CHECK(_near(_item(layout, "b")->x, 0.25f) && layout.count == 3u);
    // Cases: a brace on the left, cells flush left, a quad between the columns.
    CHECK(_set("\\begin{cases} 1 & x \\\\ 0 & yy \\end{cases}", false, layout));
    CHECK(_item(layout, nullptr, ItemKind::Delimiter)->shape == static_cast<uint8_t>(Delim::LeftBrace));
    CHECK(layout.items[layout.count - 1u].kind == ItemKind::Text);
    CHECK(_near(_item(layout, "y")->x, _item(layout, "x")->x) && _near(_item(layout, "x")->x - _item(layout, "1")->x, 1.5f));
    // Aligned: right then left columns, the relation spaced as after an Ord.
    CHECK(_set("\\begin{aligned} a &= b \\\\ cc &= d \\end{aligned}", false, layout));
    CHECK(_near(_item(layout, "a")->x, 0.5f) && _near(_item(layout, "=")->x, 1.0f + 5.0f / 18.0f));
    CHECK(_near(_item(layout, "=", ItemKind::Text, 1)->x, 1.0f + 5.0f / 18.0f));
}

static void _accents()
{
    Layout layout;
    CHECK(_set("\\hat{x}", false, layout));
    auto hat = _item(layout, nullptr, ItemKind::Accent);
    CHECK(hat->shape == static_cast<uint8_t>(Accent::Hat) && _near(hat->y, 0.511f));
    CHECK(_near(hat->x + hat->width * 0.5f, 0.25f) && layout.items[0].kind == ItemKind::Accent);
    CHECK(_set("\\hat{A}", false, layout));
    CHECK(_near(_item(layout, nullptr, ItemKind::Accent)->y, 0.78f));
    CHECK(_set("\\overline{x} \\underline{y}", false, layout));
    CHECK(_near(_item(layout, nullptr, ItemKind::Rule)->y, 0.431f + 0.12f));
    CHECK(_near(_item(layout, nullptr, ItemKind::Rule, 1)->y, -(0.2f + 0.16f)));
    CHECK(_set("\\widehat{xyz}", false, layout));
    CHECK(_near(_item(layout, nullptr, ItemKind::Accent)->width, 1.56f));
}

static void _fallbacks()
{
    Layout layout;
    // A glyph the font lacks keeps room for itself.
    CHECK(_set("\\mathbb{R}", false, layout));
    CHECK(layout.count == 1u && _near(layout.width, 0.5f));
    // A glyph that cannot be measured fails the formula, naming it.
    Parse parse;
    CHECK(_parse("a + ☃", parse));
    FakeMetrics metrics;
    char error[256] = {};
    CHECK(!tex::detail::layout(parse, false, metrics, layout, error, sizeof(error)));
    CHECK(std::strstr(error, "the font cannot set '☃'") != nullptr);
}

int main()
{
    _atoms();
    _structures();
    _errors();
    _alphabets();
    _fractions();
    _scripts();
    _operators();
    _radicals();
    _fences();
    _spacing();
    _arrays();
    _accents();
    _fallbacks();
    if (failures) std::fprintf(stderr, "%d tex checks failed\n", failures);
    return failures ? 1 : 0;
}
