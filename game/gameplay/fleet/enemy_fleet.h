#pragma once

#include "enemy_ship.h"

#include <span>
#include <vector>

namespace game::fleet
{
class EnemyFleet final
{
public:
    void configure(std::span<EnemyShip* const> ships);
    void clear() noexcept;

    [[nodiscard]] EnemyShip* flagship() const noexcept { return _flagship; }
    [[nodiscard]] std::span<EnemyShip* const> ships() const noexcept
    {
        return {_ships.data(), _ships.size()};
    }
    [[nodiscard]] EnemyShip* find_ship(elysia::physics::ColliderId collider) const noexcept;
    [[nodiscard]] int living_escort_count() const noexcept;
    [[nodiscard]] bool flagship_shield_active() const noexcept;
    [[nodiscard]] bool flagship_defeated() const noexcept;
    [[nodiscard]] game::projectile::ProjectileImpactResolution resolve_projectile_impact(
        EnemyShip& target, const game::projectile::ProjectileImpact& impact);

private:
    void refresh_flagship_shield() noexcept;

    std::vector<EnemyShip*> _ships;
    EnemyShip* _flagship = nullptr;
};
}
