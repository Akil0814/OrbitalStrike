#include "game_level_catalog.h"

#include <stdexcept>

namespace game::level
{
namespace
{
constexpr elysia::core::Rect kActivityBounds{0.0f, 0.0f, 2200.0f, 3600.0f};
constexpr elysia::core::Rect kCameraBounds{0.0f, 0.0f, 2200.0f, 3450.0f};
constexpr elysia::core::Rect kFleetBounds{200.0f, 150.0f, 1800.0f, 1300.0f};

game::objects::RadialForceConfig radial_force(
    game::objects::RadialForceMode mode, float strength, float range,
    float minimum_distance, float maximum_force)
{
    return {.mode = mode, .strength = strength, .maximum_range = range,
            .minimum_distance = minimum_distance, .maximum_force = maximum_force};
}

game::objects::EnemyShipMotionConfig ship_motion(float mass)
{
    return {.mass = mass, .linear_damping = 1.5f, .movement_bounds = kFleetBounds};
}
}

const GameLevelDefinition& GameLevelCatalog::get(GameLevelId level_id)
{
    using namespace game::objects;
    static const GameLevelDefinition prototype{
        .activity_bounds = kActivityBounds,
        .camera = {.bounds = kCameraBounds, .initial_zoom = 0.6f,
                   .minimum_zoom = 0.35f, .maximum_zoom = 1.6f,
                   .pan_speed = 800.0f, .cannon_screen_offset_ratio = 0.3f},
        .launch = {.minimum_power = 500.0f, .maximum_power = 1700.0f,
                   .initial_power = 1050.0f, .adjustment_rate = 350.0f,
                   .bullet_lifetime_seconds = 14.0},
        .moon_cell = {
            .moon_center = {1100.0f, 4350.0f}, .moon_radius = 1050.0f,
            .cannon_pivot = {1100.0f, 3250.0f}, .cannon_base_size = {120.0f, 70.0f},
            .barrel_length = 140.0f, .barrel_thickness = 20.0f,
            .radial_force = radial_force(RadialForceMode::Attract, 18.0f, 1500.0f, 1050.0f, 25.0f)},
        .ships = {
            EnemyShipConfig{
                .center = {1100.0f, 420.0f}, .facing = {0.0f, 1.0f},
                .length = 190.0f, .width = 160.0f, .collision_radius = 95.0f,
                .role = ShipRole::Flagship, .ability = ShipAbility::None,
                .hit_points = 5, .knockback_impulse = 600.0f,
                .motion = ship_motion(80.0f), .color = {205, 58, 72}},
            EnemyShipConfig{
                .center = {650.0f, 950.0f}, .facing = {0.0f, 1.0f},
                .length = 116.0f, .width = 88.0f, .collision_radius = 58.0f,
                .role = ShipRole::Escort, .ability = ShipAbility::RepulsionField,
                .hit_points = 2,
                .radial_force = radial_force(RadialForceMode::Repel, 58.0f, 650.0f, 120.0f, 105.0f),
                .knockback_impulse = 1200.0f, .motion = ship_motion(25.0f),
                .color = {218, 82, 224}},
            EnemyShipConfig{
                .center = {1550.0f, 1100.0f}, .facing = {0.0f, 1.0f},
                .length = 116.0f, .width = 88.0f, .collision_radius = 58.0f,
                .role = ShipRole::Escort, .ability = ShipAbility::ShieldProjector,
                .hit_points = 2, .knockback_impulse = 1200.0f,
                .motion = ship_motion(25.0f), .color = {72, 154, 232}}
        },
        .anomalies = {
            SpaceAnomalyConfig{
                .kind = SpaceAnomalyKind::GravityWell, .center = {700.0f, 1850.0f},
                .visual_radius = 125.0f,
                .radial_force = radial_force(RadialForceMode::Attract, 78.0f, 900.0f, 150.0f, 130.0f),
                .color = {76, 148, 255}},
            SpaceAnomalyConfig{
                .kind = SpaceAnomalyKind::RepulsionField, .center = {1500.0f, 2500.0f},
                .visual_radius = 120.0f,
                .radial_force = radial_force(RadialForceMode::Repel, 66.0f, 760.0f, 140.0f, 115.0f),
                .color = {212, 78, 224}}
        }};

    switch (level_id)
    {
    case GameLevelId::Prototype: return prototype;
    }
    throw std::invalid_argument("Unknown GameLevelId.");
}
}
