#ifndef VIS_CODE_H
#define VIS_CODE_H

#include <cstdint>

namespace vis::detail
{

// How a piece of source code reads, which picks its color.
enum struct CodeClass : uint8_t
{
    Plain = 0,
    Keyword,
    String,
    Number,
    Comment,
    Function,
    Type,
    Operator
};

enum struct CodeLanguage : uint8_t
{
    Plain = 0,
    Lua,
    Python,
    JavaScript,
    TypeScript,
    C,
    Cpp
};

// A token of one line: its bytes start at offset and run for length.
struct CodeToken
{
    uint32_t offset = 0;
    uint32_t length = 0;
    CodeClass kind = CodeClass::Plain;
};

// What a line leaves open for the next one: a block comment or a long string, and the
// delimiter that closes it.
struct CodeState
{
    CodeClass open = CodeClass::Plain;
    char close[8] = {};
    uint32_t closeLength = 0;
};

// The language of a name such as "lua" or "typescript", false for an unknown name.
bool codeLanguage(const char* name, CodeLanguage& output) noexcept;

// Splits one line, without its line feed, into tokens and returns how many it has, writing
// at most capacity of them. Blanks between tokens belong to none; a comment or string keeps
// the blanks inside it. state carries block comments and long strings across lines.
uint32_t codeTokens(CodeLanguage language, const char* line, uint32_t size, CodeState& state,
                    CodeToken* output, uint32_t capacity) noexcept;

}  // namespace vis::detail

#endif
