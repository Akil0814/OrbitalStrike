#pragma once

#include "engine/core/game_object.h"
#include "engine/core/render/color.h"
#include "engine/physics/contracts/physics_participant.h"

namespace game::objects
{
enum class CelestialFaction : unsigned char
{
    Player,
    Enemy,
    Neutral
};

struct CelestialBodyConfig
{
    elysia::core::Vector2 center{};
    float radius = 64.0f;
    CelestialFaction faction = CelestialFaction::Neutral;
    int hit_points = 1;
    float gravity_strength = 16.0f;
    elysia::core::Color color{};
};

class CelestialBody final : public elysia::core::GameObject,
                            public elysia::physics::PhysicsParticipant
{
public:
    explicit CelestialBody(CelestialBodyConfig config);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

    [[nodiscard]] CelestialFaction faction() const noexcept { return _config.faction; }
    [[nodiscard]] float radius() const noexcept { return _config.radius; }
    [[nodiscard]] float gravity_strength() const noexcept { return _config.gravity_strength; }
    [[nodiscard]] int hit_points() const noexcept { return _hit_points; }
    [[nodiscard]] bool is_destroyed_by_damage() const noexcept { return _hit_points <= 0; }
    [[nodiscard]] elysia::core::Rect circle_bounds() const noexcept { return world_rect(); }
    [[nodiscard]] bool apply_damage(int amount) noexcept;

private:
    CelestialBodyConfig _config;
    elysia::physics::Collider _collider{};
    int _hit_points = 1;
};
} // namespace game::objects
