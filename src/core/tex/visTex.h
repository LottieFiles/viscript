// The tex module's private types: parsed nodes, glyph metrics and the laid-out items.
#ifndef _VIS_TEX_PRIVATE_H_
#define _VIS_TEX_PRIVATE_H_

#include <cstdint>
#include <cstring>

namespace vis::tex::detail
{

// The longest formula the module sets, in bytes of TeX source.
static constexpr uint32_t SourceLimit = 16384u;

// The greater and the lesser of a and b; b when they compare equal or either is NaN.
static inline float _max(float a, float b)
{
    return a > b ? a : b;
}

static inline float _min(float a, float b)
{
    return a < b ? a : b;
}

// Whether c is an ASCII letter, as the letters of a control word are.
static inline bool _letter(uint32_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Whether the length bytes at text spell name.
static inline bool _named(const char* text, uint32_t length, const char* name)
{
    return std::strlen(name) == length && std::memcmp(text, name, length) == 0;
}

// The classes of math atoms, which decide the space between neighbors, as The TeXbook gives
// them in its chapter on math spacing.
enum struct Class : uint8_t
{
    Ord = 0,
    Op,
    Bin,
    Rel,
    Open,
    Close,
    Punct,
    Inner
};

// The alphabet a glyph is set in.
enum struct Variant : uint8_t
{
    Upright = 0,
    Italic,
    Bold,
    DoubleStruck,
    Script
};

enum struct Kind : uint8_t
{
    // One symbol: text and length give its UTF-8.
    Glyph = 0,
    // Letters set as one word, such as \sin or the string of \text{...}.
    Word,
    // A braced list, set as one atom: body.
    Group,
    // \frac, \dfrac, \tfrac and \binom: body over extra.
    Fraction,
    // \sqrt: body under the sign, extra the index or none.
    Root,
    // \left ... \right: body between the delimiters left and right.
    Fence,
    // A delimiter of a fixed size, as \big( sets it: left, at size value in ems.
    Delimiter,
    // An accent, as accent names, over or under body.
    Accent,
    // A kern of value ems, or of value mu when mu is set.
    Space,
    // An environment, as environment names: body is its first row.
    Array,
    // A row of an array: body is its first cell.
    Row,
    // A cell of a row: body is its list.
    Cell,
    // \displaystyle and its kin: the rest of the list is set in style.
    Style
};

enum struct Delim : uint8_t
{
    None = 0,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Vert,
    DoubleVert,
    LeftAngle,
    RightAngle,
    LeftFloor,
    RightFloor,
    LeftCeil,
    RightCeil,
    Slash,
    Backslash
};

enum struct Accent : uint8_t
{
    Hat = 0,
    Check,
    Tilde,
    Bar,
    Vec,
    Dot,
    Ddot,
    WideHat,
    WideTilde,
    Overline,
    Underline
};

enum struct Environment : uint8_t
{
    Matrix = 0,
    PMatrix,
    BMatrix,
    BraceMatrix,
    VMatrix,
    DoubleVMatrix,
    Cases,
    Aligned
};

// Styles in the order The TeXbook names them: display, text, script and scriptscript.
enum struct Level : uint8_t
{
    Display = 0,
    Text,
    Script,
    ScriptScript
};

static constexpr int32_t None = -1;

// Big operators that take their limits above and below in display style.
static constexpr uint8_t FlagLimits = 1u;
// \limits or \nolimits was given after the operator.
static constexpr uint8_t FlagLimitsSet = 2u;
// A big operator, drawn larger in display style.
static constexpr uint8_t FlagLarge = 4u;
// A fraction without a bar, as \binom sets it.
static constexpr uint8_t FlagNoBar = 8u;
// A space in math units, 1/18 of the em of the style.
static constexpr uint8_t FlagMu = 16u;
// An integral, whose limits stand beside it and which is drawn taller.
static constexpr uint8_t FlagIntegral = 32u;

struct Node
{
    Kind kind = Kind::Glyph;
    Class cls = Class::Ord;
    Variant variant = Variant::Upright;
    uint8_t flags = 0;
    // Fence delimiters, the delimiter of Kind::Delimiter, the accent, environment or style.
    uint8_t left = 0;
    uint8_t right = 0;
    // Fraction: the style it forces (Level + 1), or 0 to follow the list.
    uint8_t style = 0;
    const char* text = nullptr;
    uint32_t length = 0;
    int32_t body = None;
    int32_t extra = None;
    int32_t sup = None;
    int32_t sub = None;
    int32_t next = None;
    // The source of the node's own tokens, as byte offsets.
    uint32_t begin = 0;
    uint32_t end = 0;
    // The closing token of a fence or array: its \right and delimiter, or its \end{...}.
    uint32_t close = 0;
    uint32_t closeEnd = 0;
    float value = 0.0f;
};

// A parsed formula: nodes in a pool, the top list starting at first.
struct Parse
{
    Node* nodes = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;
    int32_t first = None;
    // Texts the parse made up, such as the primes of f''; nodes point into it.
    char* strings = nullptr;
    uint32_t stringCount = 0;
    uint32_t stringCapacity = 0;

    Parse() = default;
    Parse(const Parse&) = delete;
    Parse& operator=(const Parse&) = delete;
    ~Parse();
};

// Parses source, size bytes of TeX math; a missing source or one past SourceLimit fails. On
// failure writes why, with the 1-based character where it happened, to error and returns false.
bool parse(const char* source, uint32_t size, Parse& output, char* error, uint32_t errorSize) noexcept;

// The UTF-8 a character takes in variant: the letters and digits of the Mathematical
// Alphanumeric Symbols block, or the character itself when variant has none for it.
uint32_t alphabet(uint32_t codepoint, Variant variant) noexcept;
uint32_t encode(uint32_t codepoint, char* output) noexcept;
uint32_t decode(const char* text, uint32_t length, uint32_t& codepoint) noexcept;

// The metrics of one glyph in ems of the formula's font size, y up from the pen on the
// baseline, as Text::measure() reports them: advance, and the box its outline covers upright.
struct Glyph
{
    float advance = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    // How far the glyph leans, as Text::slant; the box above leaves it out.
    float slant = 0.0f;
    // Where the outlines reach along the baseline as they are drawn, leaning by slant: left and
    // right for an upright glyph. The layout keeps to the upright box.
    float inkLeft = 0.0f;
    float inkRight = 0.0f;
    // No font has a glyph for some of the text, which the renderer leaves out.
    bool missing = false;
    // The font lacks the letters of the variant asked for, so the text is set in its own
    // letters, leaning by slant where the variant is italic.
    bool plain = false;
};

struct Metrics
{
    virtual ~Metrics() = default;
    // Writes the metrics of text drawn as variant, and returns false when they cannot be had.
    virtual bool glyph(const char* text, uint32_t length, Variant variant, Glyph& output) noexcept = 0;
};

enum struct ItemKind : uint8_t
{
    // A glyph or word: its pen at x, y on its baseline, scale times the formula's size, its
    // outline height above the baseline and depth below it.
    Text = 0,
    // A filled box from x, y up and right by width and height.
    Rule,
    // A delimiter, as Delim shape, in the box from x, y by width and height.
    Delimiter,
    // A radical sign in the box from x, y by width and height, its strokes thickness wide; the
    // bar over the radicand is a rule of its own.
    Radical,
    // An accent, as Accent shape, in the box from x, y by width and height.
    Accent
};

struct Item
{
    ItemKind kind = ItemKind::Text;
    uint8_t shape = 0;
    Variant variant = Variant::Upright;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float depth = 0.0f;
    // The size of a text; for a shape, the size of the style it is set in.
    float scale = 1.0f;
    float thickness = 0.0f;
    float slant = 0.0f;
    // For a text, the box its outlines cover as they are drawn, leaning by slant, from its pen:
    // left and right along its baseline, top and bottom as heights above it. Height and depth
    // above keep to zero or more, as TeX's boxes do, and only they take part in the layout.
    float inkLeft = 0.0f;
    float inkRight = 0.0f;
    float inkTop = 0.0f;
    float inkBottom = 0.0f;
    const char* text = nullptr;
    uint32_t length = 0;
    // The source of the token the item came from, as byte offsets.
    uint32_t begin = 0;
    uint32_t end = 0;
};

// A formula set by layout(), in ems of its font size, from the left end of its baseline.
struct Layout
{
    Item* items = nullptr;
    uint32_t count = 0;
    uint32_t capacity = 0;
    float width = 0.0f;
    float height = 0.0f;
    float depth = 0.0f;

    Layout() = default;
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;
    ~Layout();
};

// Sets the parsed formula in display or text style. False when out of memory or when the
// metrics fail for a glyph, with why in error.
bool layout(const Parse& parse, bool display, Metrics& metrics, Layout& output, char* error,
            uint32_t errorSize) noexcept;

}  // namespace vis::tex::detail

#endif
