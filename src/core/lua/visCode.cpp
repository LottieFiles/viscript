// Tokenizes source code for code blocks: the keywords, strings, numbers and comments of each
// language the grammars name, one line at a time.
#include <cstring>

#include "visCode.h"

namespace vis::detail
{

namespace
{

struct Words
{
    const char* const* items = nullptr;
    uint32_t count = 0;
};

template <uint32_t N>
constexpr Words _words(const char* const (&items)[N])
{
    return {items, N};
}

constexpr const char* LUA_KEYWORDS[] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if", "in",
    "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"};
constexpr const char* LUA_TYPES[] = {
    "self", "print", "pairs", "ipairs", "require", "tostring", "tonumber", "type", "select",
    "error", "assert", "pcall", "xpcall", "setmetatable", "getmetatable", "rawget", "rawset",
    "rawequal", "rawlen", "next", "unpack", "math", "string", "table", "os", "io", "coroutine",
    "utf8", "vis"};
constexpr const char* PYTHON_KEYWORDS[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class",
    "continue", "def", "del", "elif", "else", "except", "finally", "for", "from", "global", "if",
    "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"};
constexpr const char* PYTHON_TYPES[] = {
    "self", "cls", "print", "len", "range", "enumerate", "zip", "map", "filter", "sorted",
    "reversed", "sum", "min", "max", "abs", "round", "open", "isinstance", "issubclass", "super",
    "int", "float", "str", "bool", "list", "dict", "set", "tuple", "bytes", "object", "type",
    "input", "repr", "iter", "next", "any", "all", "getattr", "setattr", "hasattr"};
constexpr const char* JS_KEYWORDS[] = {
    "async", "await", "break", "case", "catch", "class", "const", "continue", "debugger",
    "default", "delete", "do", "else", "export", "extends", "false", "finally", "for", "from",
    "function", "if", "import", "in", "instanceof", "let", "new", "null", "of", "return",
    "static", "super", "switch", "this", "throw", "true", "try", "typeof", "undefined", "var",
    "void", "while", "with", "yield"};
constexpr const char* JS_TYPES[] = {"console", "document", "globalThis", "window"};
constexpr const char* TS_KEYWORDS[] = {
    "abstract", "as", "declare", "enum", "implements", "infer", "interface", "is", "keyof",
    "namespace", "private", "protected", "public", "readonly", "satisfies", "type"};
constexpr const char* TS_TYPES[] = {
    "any", "bigint", "boolean", "never", "number", "object", "string", "symbol", "unknown"};
constexpr const char* C_KEYWORDS[] = {
    "auto", "break", "case", "const", "continue", "default", "do", "else", "enum", "extern",
    "false", "for", "goto", "if", "inline", "register", "restrict", "return", "sizeof", "static",
    "struct", "switch", "true", "typedef", "union", "volatile", "while", "NULL"};
constexpr const char* C_TYPES[] = {
    "bool", "char", "double", "float", "int", "long", "short", "signed", "unsigned", "void",
    "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t", "int8_t", "int16_t", "int32_t",
    "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "wchar_t", "FILE"};
constexpr const char* CPP_KEYWORDS[] = {
    "alignas", "alignof", "catch", "class", "concept", "constexpr", "consteval", "constinit",
    "const_cast", "co_await", "co_return", "co_yield", "decltype", "delete", "dynamic_cast",
    "explicit", "export", "final", "friend", "mutable", "namespace", "new", "noexcept",
    "nullptr", "operator", "override", "private", "protected", "public", "reinterpret_cast",
    "requires", "static_assert", "static_cast", "template", "this", "thread_local", "throw",
    "try", "typeid", "typename", "using", "virtual"};
constexpr const char* CPP_TYPES[] = {
    "std", "string", "string_view", "vector", "array", "map", "unordered_map", "set", "pair",
    "optional", "unique_ptr", "shared_ptr"};

struct Grammar
{
    const char* lineComment = nullptr;
    const char* blockOpen = nullptr;
    const char* blockClose = nullptr;
    // Strings in single or double quotes.
    bool quotes = false;
    // Lua long brackets: [[strings]], --[[comments]] and their [==[levels]==].
    bool longBrackets = false;
    // Python's triple-quoted strings and its r, b, f and u prefixes.
    bool tripleQuotes = false;
    // JavaScript template literals, which may span lines.
    bool backticks = false;
    // C preprocessor lines.
    bool directives = false;
    // Capitalized names such as Vector are types; names in capitals only are not.
    bool capitalTypes = false;
    // JavaScript names may hold $.
    bool dollar = false;
    Words keywords;
    Words types;
    Words moreKeywords;
    Words moreTypes;
};

Grammar _grammar(CodeLanguage language)
{
    Grammar grammar;
    switch (language) {
        case CodeLanguage::Plain: break;
        case CodeLanguage::Lua:
            grammar.lineComment = "--";
            grammar.quotes = true;
            grammar.longBrackets = true;
            grammar.keywords = _words(LUA_KEYWORDS);
            grammar.types = _words(LUA_TYPES);
            break;
        case CodeLanguage::Python:
            grammar.lineComment = "#";
            grammar.quotes = true;
            grammar.tripleQuotes = true;
            grammar.capitalTypes = true;
            grammar.keywords = _words(PYTHON_KEYWORDS);
            grammar.types = _words(PYTHON_TYPES);
            break;
        case CodeLanguage::JavaScript:
        case CodeLanguage::TypeScript:
            grammar.lineComment = "//";
            grammar.blockOpen = "/*";
            grammar.blockClose = "*/";
            grammar.quotes = true;
            grammar.backticks = true;
            grammar.capitalTypes = true;
            grammar.dollar = true;
            grammar.keywords = _words(JS_KEYWORDS);
            grammar.types = _words(JS_TYPES);
            if (language == CodeLanguage::TypeScript) {
                grammar.moreKeywords = _words(TS_KEYWORDS);
                grammar.moreTypes = _words(TS_TYPES);
            }
            break;
        case CodeLanguage::C:
        case CodeLanguage::Cpp:
            grammar.lineComment = "//";
            grammar.blockOpen = "/*";
            grammar.blockClose = "*/";
            grammar.quotes = true;
            grammar.directives = true;
            grammar.capitalTypes = true;
            grammar.keywords = _words(C_KEYWORDS);
            grammar.types = _words(C_TYPES);
            if (language == CodeLanguage::Cpp) {
                grammar.moreKeywords = _words(CPP_KEYWORDS);
                grammar.moreTypes = _words(CPP_TYPES);
            }
            break;
    }
    return grammar;
}

bool _contains(const Words& words, const char* word, uint32_t length)
{
    for (auto i = 0u; i < words.count; i++) {
        auto item = words.items[i];
        if (std::strlen(item) == length && std::memcmp(item, word, length) == 0) return true;
    }
    return false;
}

bool _starts(const char* line, uint32_t at, uint32_t size, const char* prefix)
{
    auto length = static_cast<uint32_t>(std::strlen(prefix));
    return at <= size && size - at >= length && std::memcmp(line + at, prefix, length) == 0;
}

uint32_t _find(const char* line, uint32_t from, uint32_t size, const char* text, uint32_t length)
{
    for (auto i = from; i < size && size - i >= length; i++) {
        if (std::memcmp(line + i, text, length) == 0) return i;
    }
    return UINT32_MAX;
}

bool _blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

bool _digit(char c)
{
    return c >= '0' && c <= '9';
}

bool _alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Letters of a name: ASCII letters, the underscore, and every byte of a UTF-8 sequence.
bool _letter(char c)
{
    return _alpha(c) || c == '_' || static_cast<unsigned char>(c) >= 0x80u;
}

bool _separator(char c)
{
    return c && std::strchr("()[]{},;", c);
}

// The level of a Lua long bracket, "[" with that many "=" and another "[", at at, or -1.
int _longBracket(const char* line, uint32_t at, uint32_t size)
{
    if (at >= size || line[at] != '[') return -1;
    auto level = 0;
    auto i = at + 1u;
    while (i < size && line[i] == '=') {
        level++;
        i++;
    }
    // The closing bracket must fit CodeState::close.
    return i < size && line[i] == '[' && level <= 6 ? level : -1;
}

uint32_t _longClose(int level, char* close)
{
    auto length = 0u;
    close[length++] = ']';
    for (auto i = 0; i < level; i++) close[length++] = '=';
    close[length++] = ']';
    return length;
}

bool _comment(const Grammar& grammar, const char* line, uint32_t at, uint32_t size)
{
    return (grammar.lineComment && _starts(line, at, size, grammar.lineComment))
           || (grammar.blockOpen && _starts(line, at, size, grammar.blockOpen));
}

CodeClass _word(const Grammar& grammar, const char* line, uint32_t begin, uint32_t end, uint32_t size)
{
    auto word = line + begin;
    auto length = end - begin;
    if (_contains(grammar.keywords, word, length) || _contains(grammar.moreKeywords, word, length)) {
        return CodeClass::Keyword;
    }
    if (_contains(grammar.types, word, length) || _contains(grammar.moreTypes, word, length)) {
        return CodeClass::Type;
    }
    auto next = end;
    while (next < size && _blank(line[next])) next++;
    if (next < size && line[next] == '(') return CodeClass::Function;
    if (grammar.capitalTypes && word[0] >= 'A' && word[0] <= 'Z') {
        for (auto i = 1u; i < length; i++) {
            if (word[i] >= 'a' && word[i] <= 'z') return CodeClass::Type;
        }
    }
    return CodeClass::Plain;
}

}  // namespace

uint32_t codeTokens(CodeLanguage language, const char* line, uint32_t size, CodeState& state,
                    CodeToken* output, uint32_t capacity) noexcept
{
    auto grammar = _grammar(language);
    auto count = 0u;
    auto emit = [&](uint32_t begin, uint32_t end, CodeClass kind) {
        while (end > begin && _blank(line[end - 1u])) end--;
        if (end <= begin) return;
        if (count < capacity) {
            output[count] = {begin, end - begin, language == CodeLanguage::Plain ? CodeClass::Plain : kind};
        }
        count++;
    };
    // Carries a comment or string from begin to its closing delimiter, searched from body, or
    // to the end of the line, which leaves it open. Returns where the line goes on.
    auto carry = [&](uint32_t begin, uint32_t body, CodeClass kind, const char* close, uint32_t length) {
        auto end = _find(line, body, size, close, length);
        if (end == UINT32_MAX) {
            emit(begin, size, kind);
            state.open = kind;
            std::memcpy(state.close, close, length);
            state.closeLength = length;
            return size;
        }
        emit(begin, end + length, kind);
        state.open = CodeClass::Plain;
        state.closeLength = 0;
        return end + length;
    };

    auto i = 0u;
    if (state.open != CodeClass::Plain) {
        while (i < size && _blank(line[i])) i++;
        char close[sizeof(state.close)];
        auto length = state.closeLength;
        std::memcpy(close, state.close, length);
        i = carry(i, i, state.open, close, length);
    }
    auto opening = i == 0u;
    while (i < size) {
        auto c = line[i];
        if (_blank(c)) {
            i++;
            continue;
        }
        auto first = opening;
        opening = false;
        // Comments.
        if (grammar.longBrackets && _starts(line, i, size, "--")) {
            auto level = _longBracket(line, i + 2u, size);
            if (level >= 0) {
                char close[sizeof(state.close)];
                auto length = _longClose(level, close);
                i = carry(i, i + 4u + static_cast<uint32_t>(level), CodeClass::Comment, close, length);
                continue;
            }
            emit(i, size, CodeClass::Comment);
            break;
        }
        if (grammar.lineComment && _starts(line, i, size, grammar.lineComment)) {
            emit(i, size, CodeClass::Comment);
            break;
        }
        if (grammar.blockOpen && _starts(line, i, size, grammar.blockOpen)) {
            i = carry(i, i + 2u, CodeClass::Comment, grammar.blockClose, 2u);
            continue;
        }
        // Lua long strings.
        if (grammar.longBrackets && c == '[') {
            auto level = _longBracket(line, i, size);
            if (level >= 0) {
                char close[sizeof(state.close)];
                auto length = _longClose(level, close);
                i = carry(i, i + 2u + static_cast<uint32_t>(level), CodeClass::String, close, length);
                continue;
            }
        }
        // A preprocessor directive, and the header an include names.
        if (grammar.directives && first && c == '#') {
            auto j = i + 1u;
            while (j < size && _blank(line[j])) j++;
            auto word = j;
            while (j < size && _letter(line[j])) j++;
            emit(i, j, CodeClass::Keyword);
            auto include = j - word == 7u && std::memcmp(line + word, "include", 7u) == 0;
            i = j;
            while (i < size && _blank(line[i])) i++;
            if (include && i < size && line[i] == '<') {
                auto end = _find(line, i + 1u, size, ">", 1u);
                auto stop = end == UINT32_MAX ? size : end + 1u;
                emit(i, stop, CodeClass::String);
                i = stop;
            }
            continue;
        }
        // Strings, with Python's prefixes and triple quotes.
        auto quote = i;
        if (grammar.tripleQuotes && _alpha(c)) {
            auto j = i;
            while (j < size && j - i < 2u && line[j] && std::strchr("rRbBfFuU", line[j])) j++;
            if (j > i && j < size && (line[j] == '"' || line[j] == '\'')) quote = j;
        }
        auto q = line[quote];
        if ((grammar.quotes && (q == '"' || q == '\'')) || (grammar.backticks && q == '`')) {
            if (grammar.tripleQuotes && size - quote >= 3u && line[quote + 1u] == q && line[quote + 2u] == q) {
                char close[3] = {q, q, q};
                i = carry(i, quote + 3u, CodeClass::String, close, 3u);
                continue;
            }
            auto j = quote + 1u;
            auto closed = false;
            while (j < size) {
                if (line[j] == '\\') {
                    j += 2u;
                    continue;
                }
                if (line[j++] == q) {
                    closed = true;
                    break;
                }
            }
            if (j > size) j = size;
            if (!closed && q == '`') {
                i = carry(i, size, CodeClass::String, "`", 1u);
                continue;
            }
            // A string left open ends with its line.
            emit(i, j, CodeClass::String);
            i = j;
            continue;
        }
        // Numbers: digits, a point, letters for bases and suffixes, and an exponent's sign.
        if (_digit(c) || (c == '.' && i + 1u < size && _digit(line[i + 1u]))) {
            auto hex = c == '0' && i + 1u < size && (line[i + 1u] == 'x' || line[i + 1u] == 'X');
            auto j = i + 1u;
            while (j < size) {
                auto d = line[j];
                if (d == '.' && j + 1u < size && line[j + 1u] == '.') break;
                if (_digit(d) || _alpha(d) || d == '_' || d == '.') {
                    j++;
                    continue;
                }
                auto e = line[j - 1u];
                if ((d == '+' || d == '-') && (hex ? e == 'p' || e == 'P' : e == 'e' || e == 'E')) {
                    j++;
                    continue;
                }
                break;
            }
            emit(i, j, CodeClass::Number);
            i = j;
            continue;
        }
        // Names.
        if (_letter(c) || (grammar.dollar && c == '$')) {
            auto j = i + 1u;
            while (j < size && (_letter(line[j]) || _digit(line[j]) || (grammar.dollar && line[j] == '$'))) j++;
            emit(i, j, _word(grammar, line, i, j, size));
            i = j;
            continue;
        }
        // Brackets and separators stand alone; other punctuation runs together, as in == or ->.
        if (_separator(c)) {
            emit(i, i + 1u, CodeClass::Operator);
            i++;
            continue;
        }
        auto j = i + 1u;
        while (j < size && !_blank(line[j]) && !_letter(line[j]) && !_digit(line[j]) && !_separator(line[j])
               && line[j] != '"' && line[j] != '\'' && line[j] != '`' && !(grammar.dollar && line[j] == '$')
               && !_comment(grammar, line, j, size) && !(grammar.longBrackets && _starts(line, j, size, "--"))) {
            j++;
        }
        emit(i, j, CodeClass::Operator);
        i = j;
    }
    return count;
}

}  // namespace vis::detail
