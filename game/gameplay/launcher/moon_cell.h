#pragma once

#include "../projectile/projectile_interaction.h"
#include "../../resources/game_texture_keys.h"

#include "engine/core/game_object.h"
#include "engine/gameplay/control/control_command.h"
#include "engine/physics/contracts/physics_participant.h"

#include <string_view>
#include <functional>

struct SDL_Texture;

namespace game::launcher
{
struct MoonCellConfig
{
    elysia::core::Vector2 moon_center{1400.0f, 3080.0f};
    float moon_radius = 600.0f;
    elysia::core::Vector2 cannon_pivot{1400.0f, 2450.0f};
    float barrel_length = 80.0f;
    std::string_view base_texture_key = game::resources::texture_keys::MoonCellBase;
    std::string_view cannon_texture_key = game::resources::texture_keys::MoonCell;
    elysia::core::Vector2 base_visual_size{1200.0f, 675.4286f};
    elysia::core::Vector2 base_visual_anchor{0.5f, 0.575f};
    elysia::core::Vector2 cannon_visual_size{232.0f, 228.0f};
    elysia::core::Vector2 cannon_visual_anchor{0.5f, 0.703f};
    game::projectile::RadialForceConfig radial_force{};
};

class MoonCell final : public elysia::core::GameObject,
                       public elysia::gameplay::ControlCommandReceiver,
                       public elysia::physics::PhysicsParticipant,
                       public game::projectile::ProjectileForceSource,
                       public game::projectile::ProjectileImpactTarget
{
public:
    explicit MoonCell(MoonCellConfig config);
    void set_control_handlers(
        std::function<void(const elysia::gameplay::ControlCommand&, double)> command,
        std::function<void(elysia::gameplay::InputCancelReason)> cancelled);
    void on_control_command(const elysia::gameplay::ControlCommand& command, double delta) override;
    void on_control_cancelled(elysia::gameplay::InputCancelReason reason) override;

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;
    void set_aim_direction(elysia::core::Vector2 direction) noexcept;

    [[nodiscard]] elysia::core::Vector2 cannon_pivot() const noexcept
    {
        return _config.cannon_pivot;
    }
    [[nodiscard]] elysia::core::Vector2 muzzle_position() const noexcept;
    [[nodiscard]] elysia::core::Vector2 aim_direction() const noexcept { return _aim_direction; }
    [[nodiscard]] elysia::core::Vector2 camera_anchor() const noexcept
    {
        return _config.cannon_pivot;
    }

    [[nodiscard]] elysia::core::Vector2 force_on(
        const game::projectile::ProjectileState& projectile) const noexcept override;
    [[nodiscard]] game::projectile::ProjectileImpactResolution resolve_projectile_impact(
        const game::projectile::ProjectileImpact& impact) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override
    {
        return physics_collider(0);
    }

private:
    MoonCellConfig _config;
    SDL_Texture* _base_texture = nullptr;
    SDL_Texture* _cannon_texture = nullptr;
    elysia::physics::Collider _collider{};
    elysia::core::Vector2 _aim_direction{0.0f, -1.0f};
    float _last_horizontal_sign = 1.0f;
    std::function<void(const elysia::gameplay::ControlCommand&, double)> _on_control;
    std::function<void(elysia::gameplay::InputCancelReason)> _on_cancelled;
};
}
