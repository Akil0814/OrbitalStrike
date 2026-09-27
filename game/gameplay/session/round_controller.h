#pragma once

namespace game::projectile { enum class ProjectileEndReason : unsigned char; }

namespace game::session
{
enum class RoundState : unsigned char
{
    Aiming,
    Flight,
    Resolving,
    Victory,
    FlagshipWarning,
    FlagshipFiring,
    Defeat
};

enum class ProjectileCompletionAction : unsigned char
{
    None,
    BeginResolution,
    ReturnToAiming,
    BeginFlagshipWarning
};

class RoundController final
{
public:
    void configure(int maximum_rounds) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool begin_projectile_flight() noexcept;
    void record_impact(bool objective_defeated) noexcept;
    [[nodiscard]] ProjectileCompletionAction finish_projectile(
        game::projectile::ProjectileEndReason reason) noexcept;
    [[nodiscard]] ProjectileCompletionAction finish_resolution() noexcept;
    [[nodiscard]] bool begin_flagship_firing() noexcept;
    [[nodiscard]] bool finish_flagship_firing() noexcept;

    [[nodiscard]] RoundState state() const noexcept { return _state; }
    [[nodiscard]] bool is_aiming() const noexcept { return _state == RoundState::Aiming; }
    [[nodiscard]] bool is_in_flight() const noexcept { return _state == RoundState::Flight; }
    [[nodiscard]] bool is_resolving() const noexcept { return _state == RoundState::Resolving; }
    [[nodiscard]] bool is_victorious() const noexcept { return _state == RoundState::Victory; }
    [[nodiscard]] bool is_flagship_warning() const noexcept
    {
        return _state == RoundState::FlagshipWarning;
    }
    [[nodiscard]] bool is_flagship_firing() const noexcept
    {
        return _state == RoundState::FlagshipFiring;
    }
    [[nodiscard]] bool is_defeated() const noexcept { return _state == RoundState::Defeat; }
    [[nodiscard]] bool is_terminal() const noexcept
    {
        return is_victorious() || is_defeated();
    }
    [[nodiscard]] int completed_rounds() const noexcept { return _completed_rounds; }
    [[nodiscard]] int maximum_rounds() const noexcept { return _maximum_rounds; }
    [[nodiscard]] int remaining_rounds() const noexcept
    {
        return _maximum_rounds - _completed_rounds;
    }

private:
    [[nodiscard]] ProjectileCompletionAction complete_round() noexcept;

    RoundState _state = RoundState::Aiming;
    int _completed_rounds = 0;
    int _maximum_rounds = 10;
};
}
