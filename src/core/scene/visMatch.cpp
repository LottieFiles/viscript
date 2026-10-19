// Pairs the pieces of a match transform: a diff of text lines, then shapes by kind and order.
#include <cstring>
#include <new>

#include "visMatch.h"
#include "../common/visHash.h"

namespace vis
{

// A line of a diff: one child of source or target, as the pieces [begin, end) below it.
struct MatchLine
{
    uint32_t begin = 0;
    uint32_t end = 0;
    // Lines share a key only when they read the same.
    uint32_t key = 0;
    // The line of the other object it pairs with, or UINT32_MAX.
    uint32_t partner = UINT32_MAX;
    // The pair is in the common subsequence, so the texts between such pairs pair by order.
    bool kept = false;
};

struct MatchLines
{
    MatchLine* data = nullptr;
    ~MatchLines() { delete[] data; }
};

struct MatchBuffer
{
    uint32_t* data = nullptr;
    ~MatchBuffer() { delete[] data; }
};

// The size of a table of indices by hash that count entries leave at most half full.
static uint32_t _slots(uint32_t count)
{
    auto size = 2u;
    while (size < 2u * count) size <<= 1u;
    return size;
}

// Gives each text the index of the first piece that reads the same as its key, and every other
// piece its own index, so that pieces share a key only when they are texts that read the same.
static bool _textKeys(MatchPiece* pieces, uint32_t count)
{
    auto size = _slots(count);
    MatchBuffer slots{new (std::nothrow) uint32_t[size]};
    if (!slots.data) return false;
    for (auto i = 0u; i < size; i++) slots.data[i] = UINT32_MAX;
    for (auto i = 0u; i < count; i++) {
        pieces[i].key = i;
        if (pieces[i].object->type() != Type::Text) continue;
        auto text = static_cast<const Text*>(pieces[i].object)->text();
        if (!text) continue;
        auto hash = uint64_t{14695981039346656037ull};
        _hash(hash, text, strlen(text));
        for (auto slot = static_cast<uint32_t>(hash) & (size - 1u);; slot = (slot + 1u) & (size - 1u)) {
            auto other = slots.data[slot];
            if (other == UINT32_MAX) {
                slots.data[slot] = i;
                break;
            }
            if (_sameText(pieces[other].object, pieces[i].object)) {
                pieces[i].key = other;
                break;
            }
        }
    }
    return true;
}

// The next text of a line from piece at on, or the line's end.
static uint32_t _nextText(const MatchPiece* pieces, const MatchLine& line, uint32_t at)
{
    while (at < line.end && pieces[at].object->type() != Type::Text) at++;
    return at;
}

// Two lines read the same when their texts, in order, read the same; other pieces do not count.
static bool _sameLine(const MatchPiece* pieces, const MatchLine& first, const MatchLine& second)
{
    auto i = _nextText(pieces, first, first.begin);
    auto j = _nextText(pieces, second, second.begin);
    for (; i < first.end && j < second.end; i = _nextText(pieces, first, i + 1u), j = _nextText(pieces, second, j + 1u)) {
        if (pieces[i].key != pieces[j].key) return false;
    }
    return i == first.end && j == second.end;
}

// Gives each line the index of the first line that reads the same as its key.
static bool _lineKeys(MatchLine* lines, uint32_t count, const MatchPiece* pieces)
{
    auto size = _slots(count);
    MatchBuffer slots{new (std::nothrow) uint32_t[size]};
    if (!slots.data) return false;
    for (auto i = 0u; i < size; i++) slots.data[i] = UINT32_MAX;
    for (auto i = 0u; i < count; i++) {
        auto hash = uint64_t{14695981039346656037ull};
        for (auto at = _nextText(pieces, lines[i], lines[i].begin); at < lines[i].end;
             at = _nextText(pieces, lines[i], at + 1u)) {
            _hash(hash, pieces[at].key);
        }
        lines[i].key = i;
        for (auto slot = static_cast<uint32_t>(hash) & (size - 1u);; slot = (slot + 1u) & (size - 1u)) {
            auto other = slots.data[slot];
            if (other == UINT32_MAX) {
                slots.data[slot] = i;
                break;
            }
            if (_sameLine(pieces, lines[other], lines[i])) {
                lines[i].key = other;
                break;
            }
        }
    }
    return true;
}

// Pairs a longest common subsequence of the keys a[aBegin, aEnd) and b[bBegin, bEnd): match[i]
// becomes the index of b that a[i] pairs with, and stays as it was for the rest. Hirschberg's
// split keeps memory to two rows of lengths, forward and backward, one longer than b each.
static void _common(const uint32_t* a, uint32_t aBegin, uint32_t aEnd, const uint32_t* b, uint32_t bBegin,
                    uint32_t bEnd, uint32_t* match, uint32_t* forward, uint32_t* backward)
{
    while (aBegin < aEnd && bBegin < bEnd && a[aBegin] == b[bBegin]) match[aBegin++] = bBegin++;
    while (aBegin < aEnd && bBegin < bEnd && a[aEnd - 1u] == b[bEnd - 1u]) match[--aEnd] = --bEnd;
    if (aBegin == aEnd || bBegin == bEnd) return;
    if (aEnd - aBegin == 1u) {
        for (auto j = bBegin; j < bEnd; j++) {
            if (a[aBegin] == b[j]) {
                match[aBegin] = j;
                break;
            }
        }
        return;
    }
    // forward[j] counts the longest common subsequence of a[aBegin, middle) and
    // b[bBegin, bBegin + j), and backward[j] that of a[middle, aEnd) and b[bBegin + j, bEnd).
    auto middle = aBegin + (aEnd - aBegin) / 2u;
    auto width = bEnd - bBegin;
    for (auto j = 0u; j <= width; j++) forward[j] = backward[j] = 0u;
    for (auto i = aBegin; i < middle; i++) {
        auto corner = 0u;
        for (auto j = 1u; j <= width; j++) {
            auto above = forward[j];
            if (a[i] == b[bBegin + j - 1u]) forward[j] = corner + 1u;
            else if (forward[j - 1u] > above) forward[j] = forward[j - 1u];
            corner = above;
        }
    }
    for (auto i = aEnd; i-- > middle;) {
        auto corner = 0u;
        for (auto j = width; j-- > 0u;) {
            auto below = backward[j];
            if (a[i] == b[bBegin + j]) backward[j] = corner + 1u;
            else if (backward[j + 1u] > below) backward[j] = backward[j + 1u];
            corner = below;
        }
    }
    auto split = 0u;
    for (auto j = 1u; j <= width; j++) {
        if (forward[j] + backward[j] > forward[split] + backward[split]) split = j;
    }
    _common(a, aBegin, middle, b, bBegin, bBegin + split, match, forward, backward);
    _common(a, middle, aEnd, b, bBegin + split, bEnd, match, forward, backward);
}

static void _pair(MatchPiece* pieces, uint32_t first, uint32_t second)
{
    pieces[first].partner = second;
    pieces[second].partner = first;
    pieces[first].same = pieces[second].same = true;
}

// Pairs two lines that read the same, text by text, passing over texts already paired.
static void _pairLines(MatchPiece* pieces, MatchLine* lines, uint32_t first, uint32_t second)
{
    auto& from = lines[first];
    auto& to = lines[second];
    from.partner = second;
    to.partner = first;
    auto i = _nextText(pieces, from, from.begin);
    auto j = _nextText(pieces, to, to.begin);
    for (; i < from.end && j < to.end; i = _nextText(pieces, from, i + 1u), j = _nextText(pieces, to, j + 1u)) {
        if (pieces[i].partner == UINT32_MAX && pieces[j].partner == UINT32_MAX) _pair(pieces, i, j);
    }
}

// The scratch rows of a diff, each as long as the most lines or pieces of its object.
struct MatchRows
{
    uint32_t* a;
    uint32_t* aPiece;
    uint32_t* match;
    uint32_t* b;
    uint32_t* bPiece;
    uint32_t* forward;
    uint32_t* backward;
};

// Pairs the unpaired texts of the source pieces [aBegin, aEnd) and the target pieces
// [bBegin, bEnd) by the longest common subsequence of their strings.
static void _pairTexts(MatchPiece* pieces, uint32_t aBegin, uint32_t aEnd, uint32_t bBegin, uint32_t bEnd,
                       const MatchRows& rows)
{
    auto n = 0u;
    for (auto i = aBegin; i < aEnd; i++) {
        if (pieces[i].partner != UINT32_MAX || pieces[i].object->type() != Type::Text) continue;
        rows.a[n] = pieces[i].key;
        rows.aPiece[n] = i;
        rows.match[n++] = UINT32_MAX;
    }
    auto m = 0u;
    for (auto j = bBegin; j < bEnd; j++) {
        if (pieces[j].partner != UINT32_MAX || pieces[j].object->type() != Type::Text) continue;
        rows.b[m] = pieces[j].key;
        rows.bPiece[m++] = j;
    }
    if (!n || !m) return;
    _common(rows.a, 0u, n, rows.b, 0u, m, rows.match, rows.forward, rows.backward);
    for (auto i = 0u; i < n; i++) {
        if (rows.match[i] != UINT32_MAX) _pair(pieces, rows.aPiece[i], rows.bPiece[rows.match[i]]);
    }
}

// Pairs the texts of source and target as a diff of lines does; see MatchOptions::diff.
Result diffMatch(const Object* source, const Object* target, MatchPiece* pieces, uint32_t sourceCount,
                    uint32_t total)
{
    auto sourceLines = source->type() == Type::Group ? source->childCount() : 1u;
    auto targetLines = target->type() == Type::Group ? target->childCount() : 1u;
    if (sourceLines > MAX_ANIMATION_GROUP || targetLines > MAX_ANIMATION_GROUP) return Result::InvalidArguments;
    auto lineCount = sourceLines + targetLines;
    auto targetCount = total - sourceCount;
    auto height = sourceLines > sourceCount ? sourceLines : sourceCount;
    auto width = targetLines > targetCount ? targetLines : targetCount;
    MatchLines lines{new (std::nothrow) MatchLine[lineCount ? lineCount : 1u]};
    MatchBuffer buffer{new (std::nothrow) uint32_t[3u * height + 4u * width + 2u]};
    if (!lines.data || !buffer.data || !_textKeys(pieces, total)) return Result::OutOfMemory;
    MatchRows rows;
    rows.a = buffer.data;
    rows.aPiece = rows.a + height;
    rows.match = rows.aPiece + height;
    rows.b = rows.match + height;
    rows.bPiece = rows.b + width;
    rows.forward = rows.bPiece + width;
    rows.backward = rows.forward + width + 1u;

    // A child is a line; an object that is not a group is one line of itself.
    auto at = 0u;
    for (auto i = 0u; i < sourceLines; i++) {
        lines.data[i].begin = at;
        at = source->type() == Type::Group ? _pieces(source->childAt(i), nullptr, at) : at + 1u;
        lines.data[i].end = at;
    }
    at = sourceCount;
    for (auto i = 0u; i < targetLines; i++) {
        lines.data[sourceLines + i].begin = at;
        at = target->type() == Type::Group ? _pieces(target->childAt(i), nullptr, at) : at + 1u;
        lines.data[sourceLines + i].end = at;
    }
    if (!_lineKeys(lines.data, lineCount, pieces)) return Result::OutOfMemory;

    // Lines that read the same and keep their order pair first.
    for (auto i = 0u; i < sourceLines; i++) {
        rows.a[i] = lines.data[i].key;
        rows.match[i] = UINT32_MAX;
    }
    for (auto j = 0u; j < targetLines; j++) rows.b[j] = lines.data[sourceLines + j].key;
    _common(rows.a, 0u, sourceLines, rows.b, 0u, targetLines, rows.match, rows.forward, rows.backward);
    for (auto i = 0u; i < sourceLines; i++) {
        if (rows.match[i] == UINT32_MAX) continue;
        _pairLines(pieces, lines.data, i, sourceLines + rows.match[i]);
        lines.data[i].kept = true;
    }

    // Each other line of target that reads as a line of source left over moved from there.
    for (auto j = sourceLines; j < lineCount; j++) {
        auto& line = lines.data[j];
        if (line.partner != UINT32_MAX || _nextText(pieces, line, line.begin) == line.end) continue;
        for (auto i = 0u; i < sourceLines; i++) {
            if (lines.data[i].partner == UINT32_MAX && lines.data[i].key == line.key) {
                _pairLines(pieces, lines.data, i, j);
                break;
            }
        }
    }

    // Between two kept lines, the texts left pair by their strings in order, so that a line
    // that changed keeps the words it shares with the line that replaced it.
    auto sourceAt = [&](uint32_t line) { return line < sourceLines ? lines.data[line].begin : sourceCount; };
    auto targetAt = [&](uint32_t line) { return line < targetLines ? lines.data[sourceLines + line].begin : total; };
    for (auto i = 0u, j = 0u;;) {
        auto next = i;
        while (next < sourceLines && !lines.data[next].kept) next++;
        auto partner = next < sourceLines ? lines.data[next].partner - sourceLines : targetLines;
        _pairTexts(pieces, sourceAt(i), sourceAt(next), targetAt(j), targetAt(partner), rows);
        if (next == sourceLines) break;
        i = next + 1u;
        j = partner + 1u;
    }
    return Result::Success;
}

// Pairs remaining circles/rectangles in order, splitting unequal counts into even runs.
// The first shape is primary; others are extras. False on allocation failure.
bool pairShapes(MatchPiece* pieces, uint32_t sourceCount, uint32_t total)
{
    MatchBuffer buffer;
    buffer.data = new (std::nothrow) uint32_t[total];
    if (!buffer.data) return false;
    const Type kinds[] = {Type::Circle, Type::Rectangle};
    for (auto kind : kinds) {
        auto sources = buffer.data;
        auto sourceShapes = 0u;
        for (auto i = 0u; i < sourceCount; i++) {
            if (pieces[i].partner == UINT32_MAX && pieces[i].object->type() == kind && !warped(pieces[i].object)) {
                sources[sourceShapes++] = i;
            }
        }
        auto targets = buffer.data + sourceShapes;
        auto targetShapes = 0u;
        for (auto i = sourceCount; i < total; i++) {
            if (pieces[i].partner == UINT32_MAX && pieces[i].object->type() == kind && !warped(pieces[i].object)) {
                targets[targetShapes++] = i;
            }
        }
        if (!sourceShapes || !targetShapes) continue;
        // Each shape of the side with more pairs with the shape of the other side whose share
        // of the order its place falls in.
        auto split = sourceShapes <= targetShapes;
        auto few = split ? sources : targets;
        auto many = split ? targets : sources;
        auto fewCount = split ? sourceShapes : targetShapes;
        auto manyCount = split ? targetShapes : sourceShapes;
        auto previous = UINT32_MAX;
        for (auto i = 0u; i < manyCount; i++) {
            auto at = static_cast<uint32_t>(static_cast<uint64_t>(i) * fewCount / manyCount);
            auto& one = pieces[few[at]];
            auto& piece = pieces[many[i]];
            piece.partner = few[at];
            if (at == previous) {
                piece.extra = true;
                continue;
            }
            one.partner = many[i];
            one.same = piece.same = true;
            previous = at;
        }
    }
    return true;
}

// The object that holds in the tree of copy the place object holds in the tree of source, or
// nullptr when object is not in it.
}  // namespace vis
