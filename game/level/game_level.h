#pragma once

#include "game_level_definition.h"

#include <span>
#include <vector>

namespace elysia::scene { class Scene; }
namespace game::objects { class ArenaBackdrop; }

namespace game::level
{
class GameLevel final
{
public:
    void build(elysia::scene::Scene& scene, const GameLevelDefinition& definition);
    void clear() noexcept;

    [[nodiscard]] game::objects::CelestialBody* player() const noexcept { return _player; }
    [[nodiscard]] game::objects::CelestialBody* enemy() const noexcept { return _enemy; }
    [[nodiscard]] std::span<game::objects::CelestialBody* const> bodies() const noexcept
    {
        return {_bodies.data(), _bodies.size()};
    }
    [[nodiscard]] const GameLevelDefinition* definition() const noexcept { return _definition; }
    [[nodiscard]] bool is_built() const noexcept { return _definition != nullptr; }

private:
    const GameLevelDefinition* _definition = nullptr;
    game::objects::ArenaBackdrop* _background = nullptr;
    game::objects::CelestialBody* _player = nullptr;
    game::objects::CelestialBody* _enemy = nullptr;
    std::vector<game::objects::CelestialBody*> _bodies;
};
}
