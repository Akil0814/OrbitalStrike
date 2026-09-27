#include "enemy_fleet.h"

#include <algorithm>
#include <stdexcept>

namespace game::objects
{
void EnemyFleet::configure(std::span<EnemyShip* const> ships)
{
    clear();
    _ships.assign(ships.begin(), ships.end());
    for (auto* ship : _ships)
    {
        if (!ship || ship->role() != ShipRole::Flagship) continue;
        if (_flagship) throw std::logic_error("EnemyFleet requires exactly one flagship.");
        _flagship = ship;
    }
    if (!_flagship) throw std::logic_error("EnemyFleet requires exactly one flagship.");
    refresh_flagship_shield();
}

void EnemyFleet::clear() noexcept
{
    if (_flagship) _flagship->set_projectile_shielded(false);
    _ships.clear();
    _flagship = nullptr;
}

EnemyShip* EnemyFleet::find_ship(elysia::physics::ColliderId collider) const noexcept
{
    if (collider == elysia::physics::InvalidColliderId) return nullptr;
    const auto found = std::ranges::find_if(_ships, [collider](const auto* ship) {
        return ship && ship->collider_id() == collider;
    });
    return found == _ships.end() ? nullptr : *found;
}

int EnemyFleet::living_escort_count() const noexcept
{
    return static_cast<int>(std::ranges::count_if(_ships, [](const auto* ship) {
        return ship && ship->role() != ShipRole::Flagship && !ship->is_defeated();
    }));
}

bool EnemyFleet::flagship_shield_active() const noexcept
{
    return std::ranges::any_of(_ships, [](const auto* ship) {
        return ship && !ship->is_defeated()
            && ship->ability() == ShipAbility::ShieldProjector;
    });
}

bool EnemyFleet::flagship_defeated() const noexcept
{
    return _flagship && _flagship->is_defeated();
}

ProjectileCollisionResult EnemyFleet::resolve_projectile_hit(
    EnemyShip& target, const ProjectileHitContext& hit)
{
    refresh_flagship_shield();
    if (&target == _flagship && flagship_shield_active())
        return {.disposition = ProjectileDisposition::Reflect, .restitution = 0.85f};

    const auto result = target.on_projectile_hit(hit);
    refresh_flagship_shield();
    return result;
}

void EnemyFleet::refresh_flagship_shield() noexcept
{
    if (_flagship) _flagship->set_projectile_shielded(flagship_shield_active());
}
}
