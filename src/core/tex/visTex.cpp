// tex::check(): parses a formula to report whether the module can set it.
#include <cstring>

#include "vis_tex.h"
#include "visTex.h"

namespace vis::tex
{

Result check(const char* source, char* error, uint32_t errorSize) noexcept
{
    // parse() refuses a missing source and one past the limit, however far past it.
    auto size = source ? std::strlen(source) : 0u;
    auto bounded = size > detail::SourceLimit ? detail::SourceLimit + 1u : static_cast<uint32_t>(size);
    detail::Parse parse;
    return detail::parse(source, bounded, parse, error, errorSize) ? Result::Success : Result::InvalidArguments;
}

}  // namespace vis::tex
