#pragma once

#include "projectile_interactor.h"

#include "engine/core/game_object.h"
#include "engine/core/render/color.h"

namespace game::objects
{
struct MoonCellConfig
{
    elysia::core::Vector2 moon_center{1100.0f, 4350.0f};
    float moon_radius = 1050.0f;
    elysia::core::Vector2 cannon_pivot{1100.0f, 3250.0f};
    elysia::core::Vector2 cannon_base_size{120.0f, 70.0f};
    float barrel_length = 140.0f;
    float barrel_thickness = 20.0f;
    RadialForceConfig radial_force{};
    elysia::core::Color moon_color{52, 62, 82};
    elysia::core::Color moon_outline_color{130, 150, 184};
    elysia::core::Color cannon_color{168, 190, 218};
};

class MoonCell final : public elysia::core::GameObject, public ProjectileInteractor
{
public:
    explicit MoonCell(MoonCellConfig config);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
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
        const ProjectileState& projectile) const noexcept override;
    [[nodiscard]] ProjectileCollisionResult on_projectile_hit(
        const ProjectileHitContext& hit) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override
    {
        return elysia::physics::InvalidColliderId;
    }

private:
    MoonCellConfig _config;
    elysia::core::Vector2 _aim_direction{0.0f, -1.0f};
    float _last_horizontal_sign = 1.0f;
};
}
