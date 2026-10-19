// What the input controller and its state agree on: the keys they know and the events they take.
#ifndef _VIS_INPUT_EVENT_H_
#define _VIS_INPUT_EVENT_H_

#include <cmath>

#include "vis_input.h"
#include "../common/visFinite.h"

namespace vis::input
{

constexpr auto KeyCount = static_cast<uint32_t>(Key::Tab) + 1u;

static inline bool validKey(Key key) noexcept
{
    return key != Key::Unknown && static_cast<uint32_t>(key) < KeyCount;
}

// A known kind at a finite, non-negative time with a finite position, delta and wheel; a key
// event names a known key, and a pointer press or release one of the 32 buttons.
static inline bool validEvent(const InputEvent& event) noexcept
{
    if (static_cast<uint8_t>(event.kind) > static_cast<uint8_t>(InputKind::KeyUp)
        || !_finite(event.position) || !_finite(event.delta) || !_finite(event.wheel)
        || !std::isfinite(event.time) || event.time < 0.0f) {
        return false;
    }
    if (event.kind == InputKind::KeyDown || event.kind == InputKind::KeyUp) {
        return validKey(event.key);
    }
    if ((event.kind == InputKind::PointerDown || event.kind == InputKind::PointerUp)
        && (event.button < 0 || event.button >= 32)) {
        return false;
    }
    return true;
}

}  // namespace vis::input

#endif
