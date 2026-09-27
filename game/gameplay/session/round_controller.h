#pragma once

namespace game::projectile { enum class ProjectileEndReason : unsigned char; }

namespace game::session
{
enum class RoundState : unsigned char
{
    Aiming,
    Flight,
    Resolving,
    Victory
};

enum class ProjectileCompletionAction : unsigned char
{
    None,
    BeginResolution,
    ReturnToAiming
};

class RoundController final
{
public:
    void reset() noexcept;
    [[nodiscard]] bool begin_projectile_flight() noexcept;
    void record_impact(bool objective_defeated) noexcept;
    [[nodiscard]] ProjectileCompletionAction finish_projectile(
        game::projectile::ProjectileEndReason reason) noexcept;
    [[nodiscard]] bool finish_resolution() noexcept;

    [[nodiscard]] RoundState state() const noexcept { return _state; }
    [[nodiscard]] bool is_aiming() const noexcept { return _state == RoundState::Aiming; }
    [[nodiscard]] bool is_in_flight() const noexcept { return _state == RoundState::Flight; }
    [[nodiscard]] bool is_resolving() const noexcept { return _state == RoundState::Resolving; }
    [[nodiscard]] bool is_victorious() const noexcept { return _state == RoundState::Victory; }

private:
    RoundState _state = RoundState::Aiming;
};
}
