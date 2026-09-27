#include "round_controller.h"
#include "../projectile/projectile.h"

namespace game::session
{
void RoundController::configure(int maximum_rounds) noexcept
{
    _maximum_rounds = maximum_rounds > 0 ? maximum_rounds : 1;
    reset();
}

void RoundController::reset() noexcept
{
    _state = RoundState::Aiming;
    _completed_rounds = 0;
}

bool RoundController::begin_projectile_flight() noexcept
{
    if (_state != RoundState::Aiming) return false;
    _state = RoundState::Flight;
    return true;
}

void RoundController::record_impact(bool objective_defeated) noexcept
{
    if (_state == RoundState::Flight && objective_defeated)
        _state = RoundState::Victory;
}

ProjectileCompletionAction RoundController::finish_projectile(
    game::projectile::ProjectileEndReason reason) noexcept
{
    if (_state != RoundState::Flight) return ProjectileCompletionAction::None;
    if (reason == game::projectile::ProjectileEndReason::Hit)
    {
        _state = RoundState::Resolving;
        return ProjectileCompletionAction::BeginResolution;
    }
    return complete_round();
}

ProjectileCompletionAction RoundController::finish_resolution() noexcept
{
    if (_state != RoundState::Resolving) return ProjectileCompletionAction::None;
    return complete_round();
}

bool RoundController::begin_flagship_firing() noexcept
{
    if (_state != RoundState::FlagshipWarning) return false;
    _state = RoundState::FlagshipFiring;
    return true;
}

bool RoundController::finish_flagship_firing() noexcept
{
    if (_state != RoundState::FlagshipFiring) return false;
    _state = RoundState::Defeat;
    return true;
}

ProjectileCompletionAction RoundController::complete_round() noexcept
{
    if (_state == RoundState::Victory) return ProjectileCompletionAction::None;
    if (_completed_rounds < _maximum_rounds) ++_completed_rounds;
    if (_completed_rounds >= _maximum_rounds)
    {
        _state = RoundState::FlagshipWarning;
        return ProjectileCompletionAction::BeginFlagshipWarning;
    }
    _state = RoundState::Aiming;
    return ProjectileCompletionAction::ReturnToAiming;
}
}
