#pragma once

#include "effect/effect.h"
#include <tuple>

namespace KWin
{

// KWin's paint result, region, and output types differ across API versions
// and between Wayland and the independently maintained X11 fork.
template<typename> struct ScreenPaintSignature;
template<typename R, typename C, typename... Args>
struct ScreenPaintSignature<R (C::*)(Args...)> {
    using Result = R;
    using Arguments = std::tuple<Args...>;
};
using ScreenPaint = ScreenPaintSignature<decltype(&Effect::paintScreen)>;
using ScreenOutput = std::tuple_element_t<4, ScreenPaint::Arguments>;

} // namespace KWin
