#pragma once

#include "engine/core/geometry/rect.h"

namespace game::level::map
{
inline constexpr elysia::core::Rect kBounds{0.0f, 0.0f, 2800.0f, 2800.0f};
inline constexpr auto kProjectileBounds = kBounds.expanded(1200.0f);
inline constexpr auto kBackdropBounds = kProjectileBounds.expanded({2000.0f, 1200.0f});
}
