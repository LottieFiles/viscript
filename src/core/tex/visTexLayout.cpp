#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "visTex.h"

// Sets a parsed formula the way TeX sets math (The TeXbook, Appendix G): atoms spaced by
// their classes, scripts, fractions and roots by the rules there, with TeX's font parameters
// scaled to the x-height of the font and the axis and rule thickness the font itself shows.

namespace vis::tex::detail
{

Layout::~Layout()
{
    delete[] items;
}

namespace
{

// The space between two atoms in mu, 18 to an em, by the class of the left atom (rows) and
// of the right one (columns): thin 3, medium 4, thick 5. A negative entry applies only in
// display and text styles; Bin pairs that cannot happen are 0.
static constexpr int8_t SPACING[8][8] = {
    {0, 3, -4, -5, 0, 0, 0, -3},
    {3, 3, 0, -5, 0, 0, 0, -3},
    {-4, -4, 0, 0, -4, 0, 0, -4},
    {-5, -5, 0, 0, -5, 0, 0, -5},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 3, -4, -5, 0, 0, 0, -3},
    {-3, -3, 0, -3, -3, -3, -3, -3},
    {-3, 3, -4, -5, -3, 0, -3, -3},
};

// TeX's \nulldelimiterspace and \scriptspace, in ems.
static constexpr float NullDelimiter = 0.12f;
static constexpr float ScriptSpace = 0.05f;
static constexpr uint32_t NoSlot = UINT32_MAX;

static float _clamp(float value, float low, float high)
{
    return value < low ? low : value > high ? high : value;
}

struct Style
{
    Level level = Level::Text;
    bool cramped = false;

    float size() const
    {
        return level <= Level::Text ? 1.0f : level == Level::Script ? 0.7f : 0.5f;
    }
    Style up() const
    {
        return {level <= Level::Text ? Level::Script : Level::ScriptScript, cramped};
    }
    Style down() const
    {
        return {up().level, true};
    }
    Style numerator() const
    {
        return {level == Level::Display ? Level::Text : level == Level::Text ? Level::Script : Level::ScriptScript,
                cramped};
    }
    Style denominator() const
    {
        return {numerator().level, true};
    }
    Style cramp() const
    {
        return {level, true};
    }
};

// The font parameters, in ems at the formula's size.
struct Font
{
    float xheight = 0.431f;
    float axis = 0.25f;
    float rule = 0.04f;
    float sup1, sup2, sup3, sub1, sub2, supDrop, subDrop;
    float num1, num2, num3, denom1, denom2;
    // TeX's big_op_spacing1 to 5: the room around the limits of a big operator.
    float limit1, limit2, limit3, limit4, limit5;
};

// A run of items set as one box: items first to last, placed from the left end of the box's
// baseline, with its width, its height above the baseline and its depth below.
struct Box
{
    uint32_t first = 0;
    uint32_t last = 0;
    float width = 0.0f;
    float height = 0.0f;
    float depth = 0.0f;
    // A lone glyph, over which an accent centers on center rather than on the box.
    bool lone = false;
    float center = 0.0f;
};

static Class _class(const Node& node)
{
    switch (node.kind) {
        case Kind::Glyph:
        case Kind::Word:
        case Kind::Delimiter: return node.cls;
        case Kind::Fraction:
        case Kind::Fence: return Class::Inner;
        case Kind::Array: {
            auto environment = static_cast<Environment>(node.left);
            return environment == Environment::Matrix || environment == Environment::Aligned ? Class::Ord : Class::Inner;
        }
        default: return Class::Ord;
    }
}

static bool _prime(const Node& node)
{
    return node.kind == Kind::Glyph && node.sup == None && node.sub == None && node.length == 3u
        && std::memcmp(node.text, "′", 3u) == 0;
}

// An item keyed by the TeX of node, which shows the node's text if it has one.
static Item _item(const Node& node)
{
    Item item;
    item.text = node.text;
    item.length = node.length;
    item.begin = node.begin;
    item.end = node.end;
    return item;
}

// The character a delimiter takes at the size its font draws it, if fonts commonly have it.
static const char* _character(Delim delim)
{
    switch (delim) {
        case Delim::LeftParen: return "(";
        case Delim::RightParen: return ")";
        case Delim::LeftBracket: return "[";
        case Delim::RightBracket: return "]";
        case Delim::LeftBrace: return "{";
        case Delim::RightBrace: return "}";
        case Delim::Vert: return "|";
        case Delim::DoubleVert: return "‖";
        case Delim::Slash: return "/";
        case Delim::Backslash: return "\\";
        default: return nullptr;
    }
}

// How wide a drawn delimiter is at the normal size, how much wider it grows for each em it
// grows taller, and how wide it gets at most, in ems of its style.
struct Spread
{
    float width;
    float grow;
    float widest;
};

static Spread _spread(Delim delim)
{
    switch (delim) {
        case Delim::LeftParen:
        case Delim::RightParen: return {0.42f, 0.1f, 0.85f};
        case Delim::LeftBracket:
        case Delim::RightBracket: return {0.36f, 0.05f, 0.6f};
        case Delim::LeftBrace:
        case Delim::RightBrace: return {0.52f, 0.1f, 0.9f};
        case Delim::LeftAngle:
        case Delim::RightAngle: return {0.42f, 0.12f, 0.9f};
        case Delim::LeftFloor:
        case Delim::RightFloor:
        case Delim::LeftCeil:
        case Delim::RightCeil: return {0.42f, 0.05f, 0.6f};
        case Delim::Vert: return {0.28f, 0.0f, 0.28f};
        case Delim::DoubleVert: return {0.46f, 0.0f, 0.46f};
        default: return {0.5f, 0.3f, 1.6f};
    }
}

struct Setter
{
    const Parse& parse;
    Metrics& metrics;
    Layout& out;
    char* error;
    uint32_t errorSize;
    bool failed = false;
    Font font;

    Setter(const Parse& parse, Metrics& metrics, Layout& out, char* error, uint32_t errorSize)
        : parse(parse), metrics(metrics), out(out), error(error), errorSize(errorSize)
    {
    }

    void fail(const char* format, ...)
    {
        if (failed) return;
        failed = true;
        if (error && errorSize) {
            va_list arguments;
            va_start(arguments, format);
            std::vsnprintf(error, errorSize, format, arguments);
            va_end(arguments);
        }
    }

    // Reads the parameters the font shows, keeping Computer Modern's where it shows none.
    void measureFont()
    {
        Glyph glyph;
        if (metrics.glyph("x", 1u, Variant::Upright, glyph) && !glyph.missing && glyph.top > 0.2f && glyph.top < 0.9f) {
            font.xheight = glyph.top;
        }
        if (metrics.glyph("+", 1u, Variant::Upright, glyph) && !glyph.missing && glyph.top > glyph.bottom) {
            font.axis = _clamp((glyph.top + glyph.bottom) * 0.5f, 0.15f, 0.45f);
        }
        if (metrics.glyph("−", 3u, Variant::Upright, glyph) && !glyph.missing && glyph.top > glyph.bottom) {
            font.rule = _clamp(glyph.top - glyph.bottom, 0.03f, 0.08f);
        }
        auto k = _clamp(font.xheight / 0.431f, 0.85f, 1.35f);
        font.sup1 = 0.413f * k;
        font.sup2 = 0.363f * k;
        font.sup3 = 0.289f * k;
        font.sub1 = 0.15f * k;
        font.sub2 = 0.247f * k;
        font.supDrop = 0.386f * k;
        font.subDrop = 0.05f * k;
        // TeX's baselines of numerators and denominators, kept at their distance from the axis.
        font.num1 = font.axis + 0.427f * k;
        font.num2 = font.axis + 0.144f * k;
        font.num3 = font.axis + 0.194f * k;
        font.denom1 = 0.936f * k - font.axis;
        font.denom2 = 0.595f * k - font.axis;
        font.limit1 = 0.111f * k;
        font.limit2 = 0.166f * k;
        font.limit3 = 0.2f * k;
        font.limit4 = 0.6f * k;
        font.limit5 = 0.1f * k;
    }

    float rule(float size) const
    {
        return font.rule * std::sqrt(size);
    }

    uint32_t place(uint32_t slot, const Item& item)
    {
        if (failed) return 0u;
        if (slot != NoSlot) {
            out.items[slot] = item;
            return slot;
        }
        if (out.count >= out.capacity) {
            auto capacity = out.capacity ? out.capacity * 2u : 64u;
            auto items = new (std::nothrow) Item[capacity];
            if (!items) {
                fail("out of memory");
                return 0u;
            }
            for (auto i = 0u; i < out.count; i++) items[i] = out.items[i];
            delete[] out.items;
            out.items = items;
            out.capacity = capacity;
        }
        out.items[out.count] = item;
        return out.count++;
    }

    // Holds the place of an item that is placed after the items that follow it, so that the
    // items keep the order of the source. A slot left as it is holds nothing.
    uint32_t reserve()
    {
        auto slot = place(NoSlot, Item{});
        return failed ? NoSlot : slot;
    }

    Box empty() const
    {
        Box box;
        box.first = box.last = out.count;
        return box;
    }

    void move(const Box& box, float dx, float dy)
    {
        for (auto i = box.first; i < box.last && i < out.count; i++) {
            out.items[i].x += dx;
            out.items[i].y += dy;
        }
    }

    // Places item as the text of glyph set at scale, in slot or after the items so far, and
    // returns the box TeX gives a character: its advance, and the height and depth of its
    // outline. Its ink is the box the outlines cover as drawn, which the scene gives its text;
    // where the metrics tell no reach, as for a character the font lacks, the upright box
    // stands in.
    Box placeText(uint32_t slot, Item item, const Glyph& glyph, float scale)
    {
        auto reach = glyph.inkLeft != 0.0f || glyph.inkRight != 0.0f;
        item.kind = ItemKind::Text;
        item.scale = scale;
        item.width = glyph.advance * scale;
        item.height = _max(glyph.top * scale, 0.0f);
        item.depth = _max(-glyph.bottom * scale, 0.0f);
        item.inkLeft = (reach ? glyph.inkLeft : glyph.left) * scale;
        item.inkRight = (reach ? glyph.inkRight : glyph.right) * scale;
        item.inkTop = glyph.top * scale;
        item.inkBottom = glyph.bottom * scale;
        auto index = place(slot, item);
        return {index, index + 1u, item.width, item.height, item.depth};
    }

    // Sets inner between the delimiter boxes left and right, with the first item of the
    // whole in slot.
    Box between(uint32_t slot, const Box& left, const Box& inner, const Box& right)
    {
        move(inner, left.width, 0.0f);
        move(right, left.width + inner.width, 0.0f);
        return {slot, out.count, left.width + inner.width + right.width,
                _max(inner.height, _max(left.height, right.height)), _max(inner.depth, _max(left.depth, right.depth))};
    }

    bool measure(const char* text, uint32_t length, Variant variant, Glyph& glyph)
    {
        if (metrics.glyph(text, length, variant, glyph)) {
            // A character the font lacks keeps some room, so the gap shows where it goes.
            if (glyph.missing && glyph.advance <= 0.0f) {
                glyph.advance = glyph.right = 0.5f;
                glyph.top = font.xheight;
            }
            return true;
        }
        fail("the font cannot set '%.*s'", static_cast<int>(length), text);
        return false;
    }

    float spacing(Class left, Class right, Style style) const
    {
        int mu = SPACING[static_cast<int>(left)][static_cast<int>(right)];
        if (mu < 0) {
            if (style.level >= Level::Script) return 0.0f;
            mu = -mu;
        }
        return static_cast<float>(mu) * style.size() / 18.0f;
    }

    // The next atom of a list after index, past spaces and style changes.
    int32_t following(int32_t index) const
    {
        for (index = parse.nodes[index].next; index != None; index = parse.nodes[index].next) {
            auto kind = parse.nodes[index].kind;
            if (kind != Kind::Space && kind != Kind::Style) return index;
        }
        return None;
    }

    // Sets a list, as TeX does an mlist: Bin atoms become Ord where nothing binary can stand,
    // and the atoms are spaced by their classes. afterOrd sets it as if an Ord came before,
    // as an aligned column after the first starts with {}.
    Box list(int32_t first, Style style, bool afterOrd = false)
    {
        auto box = empty();
        auto x = 0.0f;
        auto atoms = 0u;
        auto kerned = false;
        auto previous = Class::Ord;
        auto any = afterOrd;
        Box single;
        for (auto index = first; index != None && !failed; index = parse.nodes[index].next) {
            auto& node = parse.nodes[index];
            if (node.kind == Kind::Style) {
                style = Style{static_cast<Level>(node.style), false};
                continue;
            }
            if (node.kind == Kind::Space) {
                x += (node.flags & FlagMu) ? node.value * style.size() / 18.0f : node.value * style.size();
                kerned = true;
                continue;
            }
            auto cls = _class(node);
            if (cls == Class::Bin) {
                if (!any || previous == Class::Bin || previous == Class::Op || previous == Class::Rel
                    || previous == Class::Open || previous == Class::Punct) {
                    cls = Class::Ord;
                } else {
                    auto next = following(index);
                    auto after = next == None ? Class::Rel : _class(parse.nodes[next]);
                    if (after == Class::Rel || after == Class::Close || after == Class::Punct) cls = Class::Ord;
                }
            }
            if (any) x += spacing(previous, cls, style);
            auto atom = this->atom(index, style);
            if (failed) break;
            move(atom, x, 0.0f);
            x += atom.width;
            box.height = _max(box.height, atom.height);
            box.depth = _max(box.depth, atom.depth);
            previous = cls;
            any = true;
            single = atom;
            atoms++;
        }
        box.last = out.count;
        box.width = x;
        if (atoms == 1u && !kerned && single.lone) {
            box.lone = true;
            box.center = single.center;
        }
        return box;
    }

    Box atom(int32_t index, Style style)
    {
        auto& node = parse.nodes[index];
        if (node.kind == Kind::Glyph && node.cls == Class::Op && (node.flags & FlagLarge)) return operation(node, style);
        auto delta = 0.0f;
        auto symbol = false;
        Box nucleus = empty();
        switch (node.kind) {
            case Kind::Glyph:
                nucleus = text(node, style, delta);
                symbol = true;
                break;
            case Kind::Word:
                nucleus = text(node, style, delta);
                if (node.cls == Class::Op && limited(node, style)) return limits(nucleus, 0.0f, node, style);
                break;
            case Kind::Group: nucleus = list(node.body, style); break;
            case Kind::Fraction: nucleus = fraction(node, style); break;
            case Kind::Root: nucleus = root(node, style); break;
            case Kind::Fence: nucleus = fence(node, style); break;
            case Kind::Accent: nucleus = accent(node, style); break;
            case Kind::Array: nucleus = array(node, style); break;
            case Kind::Delimiter: {
                // \big and its kin give the size in ems; the bare angles and floors take the
                // normal size.
                nucleus = delimiter(NoSlot, static_cast<Delim>(node.left), node.value * style.size(), style, node.begin,
                                    node.end);
                break;
            }
            default: break;
        }
        if (failed) return nucleus;
        if (node.sup == None && node.sub == None) {
            nucleus.width += delta;
            return nucleus;
        }
        return scripts(nucleus, delta, symbol, node, style);
    }

    // A glyph or a word, its pen at the origin. delta is how far its outline reaches past
    // its advance, TeX's italic correction.
    Box text(const Node& node, Style style, float& delta)
    {
        auto size = style.size();
        Glyph glyph;
        if (!measure(node.text, node.length, node.variant, glyph)) return empty();
        auto item = _item(node);
        item.variant = glyph.plain ? Variant::Upright : node.variant;
        item.slant = glyph.slant;
        auto box = placeText(NoSlot, item, glyph, size);
        // A leaning glyph reaches further right at its top and left at its bottom.
        auto right = glyph.right + glyph.slant * _max(glyph.top, 0.0f);
        auto left = glyph.left + glyph.slant * _min(glyph.bottom, 0.0f);
        delta = _max(right - glyph.advance, 0.0f) * size;
        box.lone = node.kind == Kind::Glyph;
        box.center = (left + right) * 0.5f * size;
        return box;
    }

    bool limited(const Node& node, Style style) const
    {
        if (node.flags & FlagLimitsSet) return (node.flags & FlagLimits) != 0u;
        return style.level == Level::Display && (node.flags & FlagLimits);
    }

    // A big operator: larger in display style, centered on the axis, its limits above and
    // below or its scripts beside it.
    Box operation(const Node& node, Style style)
    {
        auto size = style.size();
        Glyph glyph;
        if (!measure(node.text, node.length, Variant::Upright, glyph)) return empty();
        auto integral = (node.flags & FlagIntegral) != 0u;
        auto display = style.level == Level::Display;
        auto ink = glyph.top - glyph.bottom;
        auto target = display ? (integral ? 1.9f : 1.35f) : (integral ? 1.1f : 0.95f);
        auto scale = size * (ink > 0.0f ? _clamp(target / ink, 1.0f, 2.2f) : 1.0f);
        auto shift = font.axis * size - (glyph.top + glyph.bottom) * 0.5f * scale;
        auto item = _item(node);
        item.y = shift;
        auto box = placeText(NoSlot, item, glyph, scale);
        auto extent = _max(glyph.advance, glyph.right) * scale;
        box.width = extent;
        box.height = _max(glyph.top * scale + shift, 0.0f);
        box.depth = _max(-(glyph.bottom * scale + shift), 0.0f);
        // An integral leans: its superscript stands at its top right, its subscript under it.
        auto delta = integral ? _max(extent - (glyph.left + 0.55f * (glyph.right - glyph.left)) * scale, 0.0f) : 0.0f;
        if (limited(node, style)) return limits(box, delta, node, style);
        box.width = extent - delta;
        if (node.sup == None && node.sub == None) {
            box.width += delta;
            return box;
        }
        return scripts(box, delta, false, node, style);
    }

    // The limits of an operator, centered above and below it; the superscript stands delta
    // to the right of the subscript.
    Box limits(Box nucleus, float delta, const Node& node, Style style)
    {
        auto size = style.size();
        auto sup = empty();
        auto sub = empty();
        auto subFirst = node.sub != None && (node.sup == None || parse.nodes[node.sub].begin < parse.nodes[node.sup].begin);
        if (subFirst) sub = list(node.sub, style.down());
        if (node.sup != None) sup = list(node.sup, style.up());
        if (!subFirst && node.sub != None) sub = list(node.sub, style.down());
        if (failed) return nucleus;
        auto width = _max(nucleus.width, _max(sup.width, sub.width));
        move(nucleus, (width - nucleus.width) * 0.5f, 0.0f);
        Box box;
        box.first = nucleus.first;
        box.last = out.count;
        box.width = width;
        box.height = nucleus.height;
        box.depth = nucleus.depth;
        if (node.sup != None) {
            auto baseline = nucleus.height + _max(font.limit1 * size, font.limit3 * size - sup.depth) + sup.depth;
            move(sup, (width - sup.width) * 0.5f + delta * 0.5f, baseline);
            box.height = baseline + sup.height + font.limit5 * size;
        }
        if (node.sub != None) {
            auto baseline = nucleus.depth + _max(font.limit2 * size, font.limit4 * size - sub.height) + sub.height;
            move(sub, (width - sub.width) * 0.5f - delta * 0.5f, -baseline);
            box.depth = baseline + sub.depth + font.limit5 * size;
        }
        return box;
    }

    // TeX's rule 18: the superscript and subscript of nucleus. symbol: the nucleus is one
    // character, whose scripts do not hang from its own top and bottom.
    Box scripts(Box nucleus, float delta, bool symbol, const Node& node, Style style)
    {
        auto size = style.size();
        auto up = style.up();
        auto down = style.down();
        auto hasSub = node.sub != None;
        if (!hasSub) {
            nucleus.width += delta;
            delta = 0.0f;
        }
        // Primes stand at the size of the nucleus, where fonts draw them high already; what
        // follows them is the superscript.
        auto primes = empty();
        auto rest = node.sup;
        auto sup = empty();
        auto sub = empty();
        auto subFirst = hasSub && (node.sup == None || parse.nodes[node.sub].begin < parse.nodes[node.sup].begin);
        if (subFirst) sub = list(node.sub, down);
        primes.first = out.count;
        while (rest != None && !failed && _prime(parse.nodes[rest])) {
            auto skip = 0.0f;
            auto prime = text(parse.nodes[rest], style, skip);
            move(prime, primes.width, 0.0f);
            primes.width += prime.width;
            primes.height = _max(primes.height, prime.height);
            primes.depth = _max(primes.depth, prime.depth);
            rest = parse.nodes[rest].next;
        }
        primes.last = out.count;
        auto hasSup = rest != None;
        if (hasSup) sup = list(rest, up);
        if (!subFirst && hasSub) sub = list(node.sub, down);
        if (failed) return nucleus;

        auto xheight = font.xheight * size;
        auto rule = this->rule(size);
        auto u = symbol ? 0.0f : nucleus.height - font.supDrop * up.size();
        auto v = symbol ? 0.0f : nucleus.depth + font.subDrop * down.size();
        if (!hasSup) {
            v = _max(v, _max(font.sub1 * size, sub.height - 0.8f * xheight));
        } else {
            auto least = style.cramped ? font.sup3 : style.level == Level::Display ? font.sup1 : font.sup2;
            u = _max(u, _max(least * size, sup.depth + 0.25f * xheight));
            if (hasSub) {
                v = _max(v, font.sub2 * size);
                auto gap = (u - sup.depth) - (sub.height - v);
                if (gap < 4.0f * rule) {
                    v = 4.0f * rule - u + sup.depth + sub.height;
                    auto lift = 0.8f * xheight - (u - sup.depth);
                    if (lift > 0.0f) {
                        u += lift;
                        v -= lift;
                    }
                }
            }
        }
        auto supAt = nucleus.width + delta;
        move(primes, supAt, 0.0f);
        move(sup, supAt + primes.width, u);
        move(sub, nucleus.width, -v);
        Box box;
        box.first = nucleus.first;
        box.last = out.count;
        box.height = _max(nucleus.height, primes.height);
        box.depth = nucleus.depth;
        auto right = supAt + primes.width;
        if (hasSup) {
            right += sup.width;
            box.height = _max(box.height, u + sup.height);
        }
        if (hasSub) {
            right = _max(right, nucleus.width + sub.width);
            box.depth = _max(box.depth, v + sub.depth);
        }
        box.width = right + ScriptSpace * size;
        return box;
    }

    // TeX's rule 15: a fraction, its bar on the axis, with the null delimiters that TeX sets
    // around it, or the parentheses of a binomial.
    Box fraction(const Node& node, Style outer)
    {
        auto style = node.style ? Style{static_cast<Level>(node.style - 1u), false} : outer;
        auto size = style.size();
        auto bar = !(node.flags & FlagNoBar);
        auto slot = reserve();
        auto numerator = list(node.body, style.numerator());
        auto denominator = list(node.extra, style.denominator());
        if (failed) return empty();
        auto display = style.level == Level::Display;
        auto rule = bar ? this->rule(size) : 0.0f;
        auto axis = font.axis * size;
        auto u = (display ? font.num1 : bar ? font.num2 : font.num3) * size;
        auto v = (display ? font.denom1 : font.denom2) * size;
        if (bar) {
            auto clearance = (display ? 3.0f : 1.0f) * rule;
            auto above = (u - numerator.depth) - (axis + rule * 0.5f);
            if (above < clearance) u += clearance - above;
            auto below = (axis - rule * 0.5f) - (denominator.height - v);
            if (below < clearance) v += clearance - below;
        } else {
            auto clearance = (display ? 7.0f : 3.0f) * this->rule(size);
            auto gap = (u - numerator.depth) - (denominator.height - v);
            if (gap < clearance) {
                u += (clearance - gap) * 0.5f;
                v += (clearance - gap) * 0.5f;
            }
        }
        auto width = _max(numerator.width, denominator.width);
        move(numerator, (width - numerator.width) * 0.5f, u);
        move(denominator, (width - denominator.width) * 0.5f, -v);
        Box inner{slot + 1u, out.count, width, _max(u + numerator.height, axis + rule * 0.5f),
                  _max(v + denominator.depth, rule * 0.5f - axis)};
        if (bar) {
            auto item = _item(node);
            item.kind = ItemKind::Rule;
            item.y = axis - rule * 0.5f;
            item.width = width;
            item.height = rule;
            item.thickness = rule;
            place(slot, item);
            move(Box{slot, out.count}, NullDelimiter * size, 0.0f);
            return {slot, out.count, width + 2.0f * NullDelimiter * size, inner.height, inner.depth};
        }
        auto fit = _max(this->fit(inner, size), (display ? 2.39f : 1.01f) * size);
        auto left = delimiter(slot, Delim::LeftParen, fit, style, node.begin, node.end);
        auto right = delimiter(NoSlot, Delim::RightParen, fit, style, node.begin, node.end);
        return between(slot, left, inner, right);
    }

    // TeX's rule 11: a root sign whose bar stands a clearance over the radicand, with the
    // index of \sqrt[n] raised over the sign's left arm as plain TeX's \root raises it.
    Box root(const Node& node, Style style)
    {
        auto size = style.size();
        auto sign = reserve();
        auto bar = reserve();
        auto index = empty();
        if (node.extra != None) index = list(node.extra, Style{Level::ScriptScript, false});
        auto body = list(node.body, style.cramp());
        if (failed) return empty();
        auto rule = this->rule(size);
        auto clearance = rule + (style.level == Level::Display ? font.xheight * size : rule) * 0.25f;
        auto top = body.height + clearance;
        auto bottom = -body.depth - 0.1f * size;
        auto height = top + rule - bottom;
        auto signWidth = size * _clamp(0.62f + 0.1f * (height / size - 1.0f), 0.62f, 0.95f);
        auto mu = size / 18.0f;
        auto signAt = 0.0f;
        auto raise = 0.0f;
        if (node.extra != None) {
            signAt = _max(5.0f * mu + index.width - 10.0f * mu, 0.0f);
            raise = 0.6f * ((top + 2.0f * rule) - _max(body.depth, -bottom));
            move(index, signAt + 10.0f * mu - index.width, raise);
        }
        auto item = _item(node);
        item.kind = ItemKind::Radical;
        item.x = signAt;
        item.y = bottom;
        item.width = signWidth;
        item.height = height;
        item.scale = size;
        item.thickness = rule;
        place(sign, item);
        item.kind = ItemKind::Rule;
        item.x = signAt + signWidth;
        item.y = top;
        item.width = body.width;
        item.height = rule;
        place(bar, item);
        move(body, signAt + signWidth, 0.0f);
        Box box;
        box.first = sign;
        box.last = out.count;
        box.width = signAt + signWidth + body.width;
        box.height = top + 2.0f * rule;
        box.depth = _max(body.depth, -bottom);
        if (node.extra != None) {
            box.height = _max(box.height, raise + index.height);
            box.depth = _max(box.depth, index.depth - raise);
        }
        return box;
    }

    // The size TeX gives the delimiters around inner: they cover it from the axis out,
    // falling short of its far end by at most \delimitershortfall.
    float fit(const Box& inner, float size) const
    {
        auto axis = font.axis * size;
        auto delta = _max(inner.height - axis, inner.depth + axis);
        return _max(2.0f * delta * 0.901f, 2.0f * delta - 0.5f * size);
    }

    Box fence(const Node& node, Style style)
    {
        auto slot = reserve();
        auto inner = list(node.body, style);
        if (failed) return empty();
        auto fit = this->fit(inner, style.size());
        auto left = delimiter(slot, static_cast<Delim>(node.left), fit, style, node.begin, node.end);
        auto right = delimiter(NoSlot, static_cast<Delim>(node.right), fit, style, node.close, node.closeEnd);
        return between(slot, left, inner, right);
    }

    // A delimiter at least size tall, centered on the axis: the font's own glyph where it is
    // tall enough, else drawn. Delim::None takes the null delimiter space.
    Box delimiter(uint32_t slot, Delim delim, float size, Style style, uint32_t begin, uint32_t end)
    {
        auto scale = style.size();
        auto box = empty();
        if (slot != NoSlot) {
            box.first = slot;
            box.last = slot + 1u;
        }
        if (delim == Delim::None) {
            box.width = NullDelimiter * scale;
            return box;
        }
        Item item;
        item.begin = begin;
        item.end = end;
        if (auto character = _character(delim)) {
            auto length = static_cast<uint32_t>(std::strlen(character));
            Glyph glyph;
            if (metrics.glyph(character, length, Variant::Upright, glyph) && !glyph.missing
                && size <= (glyph.top - glyph.bottom) * scale * 1.01f) {
                item.text = character;
                item.length = length;
                return placeText(slot, item, glyph, scale);
            }
        }
        auto height = _max(size, scale);
        auto spread = _spread(delim);
        auto taller = height / scale - 1.0f;
        auto axis = font.axis * scale;
        item.kind = ItemKind::Delimiter;
        item.shape = static_cast<uint8_t>(delim);
        item.scale = scale;
        item.y = axis - height * 0.5f;
        item.width = _min(spread.width + spread.grow * taller, spread.widest) * scale;
        item.height = height;
        item.thickness = rule(scale) * (1.3f + 0.15f * _min(taller, 3.0f));
        auto index = place(slot, item);
        box.first = index;
        box.last = index + 1u;
        box.width = item.width;
        box.height = axis + height * 0.5f;
        box.depth = height * 0.5f - axis;
        return box;
    }

    // TeX's rules 9, 10 and 12: an accent over its body, centered on a lone glyph's outline
    // and at least as high as the x-height; \overline and \underline as rules.
    Box accent(const Node& node, Style style)
    {
        auto size = style.size();
        auto which = static_cast<Accent>(node.left);
        auto slot = reserve();
        auto body = list(node.body, which == Accent::Underline ? style : style.cramp());
        if (failed) return empty();
        auto rule = this->rule(size);
        auto item = _item(node);
        item.scale = size;
        item.thickness = rule;
        Box box;
        box.first = slot;
        box.last = out.count;
        box.width = body.width;
        box.height = body.height;
        box.depth = body.depth;
        if (which == Accent::Overline || which == Accent::Underline) {
            item.kind = ItemKind::Rule;
            item.width = body.width;
            item.height = rule;
            if (which == Accent::Overline) {
                item.y = body.height + 3.0f * rule;
                box.height = body.height + 5.0f * rule;
            } else {
                item.y = -(body.depth + 4.0f * rule);
                box.depth = body.depth + 5.0f * rule;
            }
            place(slot, item);
            return box;
        }
        auto narrow = _clamp(body.width * 0.9f, 0.3f * size, 0.5f * size);
        auto width = narrow;
        auto height = 0.12f * size;
        switch (which) {
            case Accent::Tilde: height = 0.1f * size; break;
            case Accent::Bar: height = rule; break;
            case Accent::Vec:
                width = _clamp(body.width, 0.36f * size, 0.5f * size);
                height = 0.18f * size;
                break;
            case Accent::Dot: width = 0.12f * size; break;
            case Accent::Ddot: width = 0.34f * size; break;
            case Accent::WideHat:
                width = body.width + 0.06f * size;
                height = _min(0.12f * size + 0.04f * width, 0.24f * size);
                break;
            case Accent::WideTilde:
                width = body.width + 0.06f * size;
                height = _min(0.1f * size + 0.03f * width, 0.2f * size);
                break;
            default: break;
        }
        auto center = body.lone ? body.center : body.width * 0.5f;
        auto bottom = _max(body.height, font.xheight * size) + 0.08f * size;
        item.kind = ItemKind::Accent;
        item.shape = static_cast<uint8_t>(which);
        item.x = center - width * 0.5f;
        item.y = bottom;
        item.width = width;
        item.height = height;
        item.thickness = rule * 1.1f;
        place(slot, item);
        box.height = _max(body.height, bottom + height);
        return box;
    }

    // The matrix, cases and aligned environments: cells in rows on struts, columns as wide
    // as their widest cell, the whole centered on the axis between its delimiters.
    Box array(const Node& node, Style outer)
    {
        auto environment = static_cast<Environment>(node.left);
        auto left = Delim::None;
        auto right = Delim::None;
        switch (environment) {
            case Environment::PMatrix: left = Delim::LeftParen, right = Delim::RightParen; break;
            case Environment::BMatrix: left = Delim::LeftBracket, right = Delim::RightBracket; break;
            case Environment::BraceMatrix: left = Delim::LeftBrace, right = Delim::RightBrace; break;
            case Environment::VMatrix: left = right = Delim::Vert; break;
            case Environment::DoubleVMatrix: left = right = Delim::DoubleVert; break;
            case Environment::Cases: left = Delim::LeftBrace; break;
            default: break;
        }
        auto fenced = left != Delim::None;
        auto slot = fenced ? reserve() : NoSlot;
        auto aligned = environment == Environment::Aligned;
        Style style{aligned ? Level::Display : Level::Text, false};
        if (outer.level >= Level::Script) style.level = outer.level;
        auto size = style.size();

        auto rows = 0u;
        auto columns = 0u;
        auto cells = 0u;
        for (auto row = node.body; row != None; row = parse.nodes[row].next) {
            auto count = 0u;
            for (auto cell = parse.nodes[row].body; cell != None; cell = parse.nodes[cell].next) count++;
            rows++;
            cells += count;
            if (count > columns) columns = count;
        }
        std::unique_ptr<Box[]> boxes(new (std::nothrow) Box[cells]);
        std::unique_ptr<float[]> widths(new (std::nothrow) float[columns]());
        std::unique_ptr<float[]> offsets(new (std::nothrow) float[columns]());
        std::unique_ptr<float[]> heights(new (std::nothrow) float[rows]());
        std::unique_ptr<float[]> depths(new (std::nothrow) float[rows]());
        if (!boxes || !widths || !offsets || !heights || !depths) {
            fail("out of memory");
            return empty();
        }
        auto stretch = environment == Environment::Cases ? 1.2f : 1.0f;
        auto content = empty();
        auto k = 0u;
        auto r = 0u;
        for (auto row = node.body; row != None && !failed; row = parse.nodes[row].next, r++) {
            heights[r] = 0.84f * stretch * size;
            depths[r] = 0.36f * stretch * size;
            auto c = 0u;
            for (auto cell = parse.nodes[row].body; cell != None && !failed; cell = parse.nodes[cell].next, c++, k++) {
                boxes[k] = list(parse.nodes[cell].body, style, aligned && (c & 1u));
                widths[c] = _max(widths[c], boxes[k].width);
                heights[r] = _max(heights[r], boxes[k].height);
                depths[r] = _max(depths[r], boxes[k].depth);
            }
        }
        if (failed) return empty();
        content.last = out.count;

        auto jot = aligned ? 0.3f * size : 0.0f;
        auto total = heights[0] + depths[rows - 1u];
        for (r = 1u; r < rows; r++) total += depths[r - 1u] + jot + heights[r];
        auto axis = font.axis * size;
        auto x = 0.0f;
        for (auto c = 0u; c < columns; c++) {
            offsets[c] = x;
            x += widths[c];
            if (c + 1u < columns) x += (aligned ? ((c & 1u) ? 2.0f : 0.0f) : 1.0f) * size;
        }
        auto baseline = axis + total * 0.5f - heights[0];
        k = 0u;
        r = 0u;
        for (auto row = node.body; row != None; row = parse.nodes[row].next, r++) {
            if (r) baseline -= depths[r - 1u] + jot + heights[r];
            auto c = 0u;
            for (auto cell = parse.nodes[row].body; cell != None; cell = parse.nodes[cell].next, c++, k++) {
                auto& box = boxes[k];
                auto room = widths[c] - box.width;
                auto shift = environment == Environment::Cases ? 0.0f : aligned ? ((c & 1u) ? 0.0f : room) : room * 0.5f;
                move(box, offsets[c] + shift, baseline);
            }
        }
        content.width = x;
        content.height = axis + total * 0.5f;
        content.depth = total * 0.5f - axis;
        if (!fenced) return content;

        auto fit = this->fit(content, size);
        auto opening = delimiter(slot, left, fit, style, node.begin, parse.nodes[node.body].begin);
        auto closing = delimiter(NoSlot, right, fit, style, node.close, node.closeEnd);
        return between(slot, opening, content, closing);
    }
};

}  // namespace

bool layout(const Parse& parse, bool display, Metrics& metrics, Layout& output, char* error, uint32_t errorSize) noexcept
{
    if (error && errorSize) error[0] = '\0';
    delete[] output.items;
    output.items = nullptr;
    output.count = output.capacity = 0u;
    output.width = output.height = output.depth = 0.0f;
    output.capacity = parse.count * 2u + 16u;
    output.items = new (std::nothrow) Item[output.capacity];
    if (!output.items) {
        output.capacity = 0u;
        if (error && errorSize) std::snprintf(error, errorSize, "out of memory");
        return false;
    }
    Setter setter(parse, metrics, output, error, errorSize);
    setter.measureFont();
    auto box = setter.list(parse.first, Style{display ? Level::Display : Level::Text, false});
    if (setter.failed) return false;
    // Drop the slots left empty, as for \left. and \right.
    auto count = 0u;
    for (auto i = 0u; i < output.count; i++) {
        if (output.items[i].kind == ItemKind::Text && !output.items[i].text) continue;
        output.items[count++] = output.items[i];
    }
    output.count = count;
    output.width = box.width;
    output.height = box.height;
    output.depth = box.depth;
    return true;
}

}  // namespace vis::tex::detail
