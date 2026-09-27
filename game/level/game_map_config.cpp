#include "game_map_config.h"

namespace game::level
{
GameMapConfig make_game_map_config(GameMapSize size)
{
    elysia::core::Vector2 dimensions;
    std::uint64_t seed = 0;
    switch (size)
    {
    case GameMapSize::Small:
        dimensions = {2400.0f, 3600.0f};
        seed = 0x534D414C4C4D4150ULL;
        break;
    case GameMapSize::Standard:
        dimensions = {2800.0f, 5200.0f};
        seed = 0x5354414E44415244ULL;
        break;
    case GameMapSize::Large:
        dimensions = {3600.0f, 6800.0f};
        seed = 0x4C415247454D4150ULL;
        break;
    }

    const elysia::core::Rect activity{0.0f, 0.0f, dimensions.x, dimensions.y};
    const auto projectile = activity.expanded(1200.0f);
    return {
        .activity_bounds = activity,
        .projectile_bounds = projectile,
        .backdrop_bounds = projectile.expanded({2000.0f, 1200.0f}),
        .starfield = {.seed = seed}};
}
}
