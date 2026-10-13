#include <cstdio>
#include <cstring>

#include "vis_tex.h"
#include "visTex.h"

namespace vis::tex
{

Result check(const char* source, char* error, uint32_t errorSize) noexcept
{
    if (error && errorSize) error[0] = '\0';
    if (!source) {
        if (error && errorSize) std::snprintf(error, errorSize, "the formula is missing");
        return Result::InvalidArguments;
    }
    detail::Parse parse;
    auto size = std::strlen(source);
    if (size > detail::SourceLimit) {
        if (error && errorSize) std::snprintf(error, errorSize, "a formula holds at most %u bytes", detail::SourceLimit);
        return Result::InvalidArguments;
    }
    return detail::parse(source, static_cast<uint32_t>(size), parse, error, errorSize) ? Result::Success
                                                                                           : Result::InvalidArguments;
}

}  // namespace vis::tex
