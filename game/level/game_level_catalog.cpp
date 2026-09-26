#include "game_level_catalog.h"

#include <stdexcept>

namespace game::level
{
namespace
{
constexpr elysia::core::Rect kPrototypeBounds{0.0f, 0.0f, 1600.0f, 1000.0f};

game::objects::RadialForceConfig attractive_force(float strength)
{
    return {
        .mode = game::objects::RadialForceMode::Attract,
        .strength = strength,
        .maximum_range = 520.0f,
        .minimum_distance = 140.0f,
        .maximum_force = 80.0f};
}

game::objects::CelestialMotionConfig movable_enemy()
{
    return {
        .dynamic = true,
        .mass = 20.0f,
        .linear_damping = 1.5f,
        .movement_bounds = kPrototypeBounds};
}
}

const GameLevelDefinition& GameLevelCatalog::get(GameLevelId level_id)
{
    static const GameLevelDefinition prototype{
        .activity_bounds = kPrototypeBounds,
        .initial_zoom = 0.8f,
        .initial_power = 700.0f,
        .player = {
            .center = {230.0f, 720.0f},
            .radius = 66.0f,
            .faction = game::objects::CelestialFaction::Player,
            .hit_points = 1,
            .receives_damage = false,
            .radial_force = attractive_force(16.0f),
            .color = {63, 145, 255}},
        .bodies = {
            {
                .center = {1320.0f, 240.0f},
                .radius = 74.0f,
                .faction = game::objects::CelestialFaction::Enemy,
                .hit_points = 3,
                .receives_damage = true,
                .radial_force = attractive_force(20.0f),
                .projectile_disposition = game::objects::ProjectileDisposition::Destroy,
                .hit_reaction = game::objects::HitReactionMode::Knockback,
                .knockback_impulse = 1200.0f,
                .motion = movable_enemy(),
                .color = {218, 70, 86}},
            {
                .center = {1260.0f, 780.0f},
                .radius = 58.0f,
                .faction = game::objects::CelestialFaction::Enemy,
                .hit_points = 2,
                .receives_damage = true,
                .radial_force = attractive_force(14.0f),
                .projectile_disposition = game::objects::ProjectileDisposition::Destroy,
                .hit_reaction = game::objects::HitReactionMode::Knockback,
                .knockback_impulse = 1200.0f,
                .motion = movable_enemy(),
                .color = {230, 112, 72}},
            {
                .center = {760.0f, 570.0f},
                .radius = 100.0f,
                .faction = game::objects::CelestialFaction::Neutral,
                .hit_points = 1,
                .receives_damage = false,
                .radial_force = attractive_force(35.0f),
                .projectile_disposition = game::objects::ProjectileDisposition::Destroy,
                .color = {126, 102, 176}}
        }};

    switch (level_id)
    {
    case GameLevelId::Prototype:
        return prototype;
    }
    throw std::invalid_argument("Unknown GameLevelId.");
}
}
