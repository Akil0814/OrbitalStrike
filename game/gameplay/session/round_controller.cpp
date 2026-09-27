#include "round_controller.h"
#include "../projectile/projectile.h"

namespace game::session
{
void RoundController::reset() noexcept
{
    _state = RoundState::Aiming;
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
    _state = RoundState::Aiming;
    return ProjectileCompletionAction::ReturnToAiming;
}

bool RoundController::finish_resolution() noexcept
{
    if (_state != RoundState::Resolving) return false;
    _state = RoundState::Aiming;
    return true;
}
}
