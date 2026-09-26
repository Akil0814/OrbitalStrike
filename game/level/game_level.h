#pragma once

#include "game_level_definition.h"

#include <span>
#include <vector>

namespace elysia::scene { class Scene; }
namespace game::objects { class ArenaBackdrop; class ProjectileInteractor; }

namespace game::level
{
class GameLevel final
{
public:
    void build(elysia::scene::Scene& scene, const GameLevelDefinition& definition);
    void clear() noexcept;

    [[nodiscard]] game::objects::CelestialBody* player() const noexcept { return _player; }
    [[nodiscard]] std::span<game::objects::CelestialBody* const> enemies() const noexcept
    {
        return {_enemies.data(), _enemies.size()};
    }
    [[nodiscard]] std::span<game::objects::ProjectileInteractor* const> interactors() const noexcept
    {
        return {_interactors.data(), _interactors.size()};
    }
    [[nodiscard]] game::objects::ProjectileInteractor* find_interactor(
        elysia::physics::ColliderId collider) const noexcept;
    [[nodiscard]] game::objects::CelestialBody* first_alive_enemy() const noexcept;
    [[nodiscard]] bool all_enemies_defeated() const noexcept;
    [[nodiscard]] int remaining_enemy_count() const noexcept;
    [[nodiscard]] int total_enemy_hit_points() const noexcept;
    [[nodiscard]] const GameLevelDefinition* definition() const noexcept { return _definition; }
    [[nodiscard]] bool is_built() const noexcept { return _definition != nullptr; }

private:
    const GameLevelDefinition* _definition = nullptr;
    game::objects::ArenaBackdrop* _background = nullptr;
    game::objects::CelestialBody* _player = nullptr;
    std::vector<game::objects::CelestialBody*> _bodies;
    std::vector<game::objects::CelestialBody*> _enemies;
    std::vector<game::objects::ProjectileInteractor*> _interactors;
};
}
