#pragma once

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"

namespace game::fleet
{
enum class FlagshipLaserPhase : unsigned char { Warning, Firing };

class FlagshipLaser final : public elysia::core::GameObject,
                            public elysia::core::Updatable
{
public:
    FlagshipLaser(elysia::core::Vector2 origin, elysia::core::Vector2 target);

    void update(double delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    void set_phase(FlagshipLaserPhase phase) noexcept;
    [[nodiscard]] FlagshipLaserPhase phase() const noexcept { return _phase; }

private:
    elysia::core::Vector2 _origin{};
    elysia::core::Vector2 _target{};
    FlagshipLaserPhase _phase = FlagshipLaserPhase::Warning;
    double _elapsed_seconds = 0.0;
};
}
