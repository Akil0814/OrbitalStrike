#include "game_level_catalog.h"

#include <array>
#include <stdexcept>

namespace game::level
{
namespace
{
constexpr elysia::core::Rect kAimingBounds{0.0f, 0.0f, 2800.0f, 5050.0f};
constexpr elysia::core::Rect kFleetBounds{200.0f, 200.0f, 2400.0f, 1700.0f};

game::projectile::RadialForceConfig radial_force(
    game::projectile::RadialForceMode mode, float strength, float range,
    float minimum_distance, float maximum_force)
{
    return {.mode = mode, .strength = strength, .maximum_range = range,
            .minimum_distance = minimum_distance, .maximum_force = maximum_force};
}

game::fleet::EnemyShipMotionConfig ship_motion(float mass)
{
    return {.mass = mass, .linear_damping = 1.5f, .movement_bounds = kFleetBounds};
}
}

std::span<const GameLevelEntry> GameLevelCatalog::entries() noexcept
{
    static constexpr std::array levels{
        GameLevelEntry{GameLevelId::FirstStrike, "Level 1 - First Strike"},
        GameLevelEntry{GameLevelId::Prototype, "Level 2 - Fleet & Anomalies"}};
    return levels;
}

const GameLevelDefinition& GameLevelCatalog::get(GameLevelId level_id)
{
    using namespace game::anomaly;
    using namespace game::fleet;
    using namespace game::launcher;
    using namespace game::projectile;
    static const GameLevelDefinition first_strike{
        .map = make_game_map_config(GameMapSize::Small),
        .camera = {.aiming_bounds = {0.0f, 0.0f, 2400.0f, 3450.0f},
                   .initial_zoom = 0.6f, .minimum_zoom = 0.35f, .maximum_zoom = 1.6f,
                   .pan_speed = 800.0f, .cannon_screen_offset_ratio = 0.3f},
        .launch = {.minimum_power = 600.0f, .maximum_power = 1900.0f,
                   .initial_power = 1200.0f, .adjustment_rate = 350.0f,
                   .projectile = {.lifetime_seconds = 20.0}},
        .mission = {.maximum_rounds = 4, .flagship_warning_seconds = 1.25,
                    .flagship_firing_seconds = 0.45},
        .moon_cell = {
            .moon_center = {1200.0f, 4350.0f}, .moon_radius = 1050.0f,
            .cannon_pivot = {1200.0f, 3250.0f}, .barrel_length = 140.0f,
            .radial_force = {}},
        .ships = {
            EnemyShipConfig{
                .center = {1200.0f, 500.0f}, .facing = {0.0f, 1.0f},
                .length = 280.0f, .width = 240.0f, .collision_radius = 140.0f,
                .role = ShipRole::Flagship, .ability = ShipAbility::None,
                .hit_points = 2, .knockback_impulse = 600.0f,
                .motion = {.mass = 80.0f, .linear_damping = 1.5f,
                           .movement_bounds = {200.0f, 200.0f, 2000.0f, 1300.0f}},
                .color = {205, 58, 72}}},
        .anomalies = {}};

    static const GameLevelDefinition prototype{
        .map = make_game_map_config(GameMapSize::Standard),
        .camera = {.aiming_bounds = kAimingBounds, .initial_zoom = 0.6f,
                   .minimum_zoom = 0.35f, .maximum_zoom = 1.6f,
                   .pan_speed = 800.0f, .cannon_screen_offset_ratio = 0.3f},
        .launch = {.minimum_power = 600.0f, .maximum_power = 1900.0f,
                   .initial_power = 1200.0f, .adjustment_rate = 350.0f,
                   .projectile = {.lifetime_seconds = 20.0}},
        .mission = {.maximum_rounds = 12, .flagship_warning_seconds = 1.25,
                    .flagship_firing_seconds = 0.45},
        .moon_cell = {
            .moon_center = {1400.0f, 5950.0f}, .moon_radius = 1050.0f,
            .cannon_pivot = {1400.0f, 4850.0f}, .barrel_length = 140.0f,
            .radial_force = radial_force(RadialForceMode::Attract, 10.0f, 1500.0f, 1050.0f, 15.0f)},
        .ships = {
            EnemyShipConfig{
                .center = {1400.0f, 500.0f}, .facing = {0.0f, 1.0f},
                .length = 280.0f, .width = 240.0f, .collision_radius = 140.0f,
                .role = ShipRole::Flagship, .ability = ShipAbility::None,
                .hit_points = 3, .knockback_impulse = 600.0f,
                .motion = ship_motion(80.0f), .color = {205, 58, 72}},
            EnemyShipConfig{
                .center = {750.0f, 1250.0f}, .facing = {0.0f, 1.0f},
                .length = 170.0f, .width = 132.0f, .collision_radius = 85.0f,
                .role = ShipRole::Escort, .ability = ShipAbility::RepulsionField,
                .hit_points = 1,
                .radial_force = radial_force(RadialForceMode::Repel, 35.0f, 550.0f, 120.0f, 65.0f),
                .knockback_impulse = 1200.0f, .motion = ship_motion(25.0f),
                .color = {218, 82, 224}},
            EnemyShipConfig{
                .center = {2050.0f, 1450.0f}, .facing = {0.0f, 1.0f},
                .length = 170.0f, .width = 132.0f, .collision_radius = 85.0f,
                .role = ShipRole::Escort, .ability = ShipAbility::ShieldProjector,
                .hit_points = 1, .knockback_impulse = 1200.0f,
                .motion = ship_motion(25.0f), .color = {72, 154, 232}}
        },
        .anomalies = {
            RadialFieldAnomalyConfig{
                .kind = SpaceAnomalyKind::GravityWell, .center = {900.0f, 2850.0f},
                .visual_radius = 125.0f,
                .radial_force = radial_force(RadialForceMode::Attract, 12.0f, 700.0f, 150.0f, 35.0f),
                .color = {76, 148, 255}},
            WormholePairConfig{
                .first_center = {2050.0f, 3900.0f},
                .second_center = {2350.0f, 2100.0f},
                .portal_radius = 75.0f,
                .exit_offset = 96.0f},
            BlackHoleConfig{
                .center = {650.0f, 2050.0f},
                .visual_radius = 125.0f,
                .event_horizon_radius = 70.0f,
                .radial_force = radial_force(
                    RadialForceMode::Attract, 120.0f, 600.0f, 60.0f, 150.0f),
                .color = {116, 74, 210}}
        }};

    switch (level_id)
    {
    case GameLevelId::FirstStrike: return first_strike;
    case GameLevelId::Prototype: return prototype;
    }
    throw std::invalid_argument("Unknown GameLevelId.");
}
}
