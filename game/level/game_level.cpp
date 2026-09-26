#include "game_level.h"

#include "../objects/arena_backdrop.h"
#include "engine/scene/scene.h"

#include <stdexcept>

namespace game::level
{
void GameLevel::build(elysia::scene::Scene& scene, const GameLevelDefinition& definition)
{
    if (is_built())
        throw std::logic_error("GameLevel is already built.");

    _definition = &definition;
    _bodies.reserve(2 + definition.neutral_bodies.size());
    _background = scene.create_and_add_object<game::objects::ArenaBackdrop>(definition.activity_bounds);
    _player = scene.create_and_add_object<game::objects::CelestialBody>(definition.player);
    _enemy = scene.create_and_add_object<game::objects::CelestialBody>(definition.enemy);

    if (_player) _bodies.push_back(_player);
    if (_enemy) _bodies.push_back(_enemy);
    for (const auto& body : definition.neutral_bodies)
    {
        auto* added = scene.create_and_add_object<game::objects::CelestialBody>(body);
        if (added) _bodies.push_back(added);
    }

    if (!_background || !_player || !_enemy || _bodies.size() != 2 + definition.neutral_bodies.size())
    {
        clear();
        throw std::runtime_error("GameLevel failed to create all scene objects.");
    }
}

void GameLevel::clear() noexcept
{
    const auto destroy = [](elysia::core::SceneObject* object) {
        if (object && !object->is_destroyed()) object->destroy();
    };
    for (auto* body : _bodies) destroy(body);
    destroy(_background);

    _bodies.clear();
    _player = nullptr;
    _enemy = nullptr;
    _background = nullptr;
    _definition = nullptr;
}
}
