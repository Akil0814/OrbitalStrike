#include "game_level.h"

#include "../objects/arena_backdrop.h"
#include "../objects/projectile_interactor.h"
#include "engine/scene/scene.h"

#include <algorithm>
#include <stdexcept>

namespace game::level
{
void GameLevel::build(elysia::scene::Scene& scene, const GameLevelDefinition& definition)
{
    if (is_built()) throw std::logic_error("GameLevel is already built.");

    _definition = &definition;
    _bodies.reserve(1 + definition.bodies.size());
    _enemies.reserve(definition.bodies.size());
    _interactors.reserve(1 + definition.bodies.size());
    _background = scene.create_and_add_object<game::objects::ArenaBackdrop>(definition.activity_bounds);
    _player = scene.create_and_add_object<game::objects::CelestialBody>(definition.player);
    if (_player)
    {
        _bodies.push_back(_player);
        _interactors.push_back(_player);
    }

    for (const auto& body_config : definition.bodies)
    {
        auto* body = scene.create_and_add_object<game::objects::CelestialBody>(body_config);
        if (!body) continue;
        _bodies.push_back(body);
        _interactors.push_back(body);
        if (body->faction() == game::objects::CelestialFaction::Enemy)
            _enemies.push_back(body);
    }

    if (!_background || !_player || _bodies.size() != 1 + definition.bodies.size()
        || _enemies.empty())
    {
        clear();
        throw std::runtime_error("GameLevel failed to create a valid set of scene objects.");
    }
}

void GameLevel::clear() noexcept
{
    const auto destroy = [](elysia::core::SceneObject* object) {
        if (object && !object->is_destroyed()) object->destroy();
    };
    for (auto* body : _bodies) destroy(body);
    destroy(_background);

    _interactors.clear();
    _enemies.clear();
    _bodies.clear();
    _player = nullptr;
    _background = nullptr;
    _definition = nullptr;
}

game::objects::ProjectileInteractor* GameLevel::find_interactor(
    elysia::physics::ColliderId collider) const noexcept
{
    if (collider == elysia::physics::InvalidColliderId) return nullptr;
    const auto found = std::ranges::find_if(_interactors, [collider](const auto* interactor) {
        return interactor && interactor->collider_id() == collider;
    });
    return found == _interactors.end() ? nullptr : *found;
}

game::objects::CelestialBody* GameLevel::first_alive_enemy() const noexcept
{
    const auto found = std::ranges::find_if(_enemies, [](const auto* enemy) {
        return enemy && !enemy->is_defeated();
    });
    return found == _enemies.end() ? nullptr : *found;
}

bool GameLevel::all_enemies_defeated() const noexcept
{
    return !_enemies.empty() && std::ranges::all_of(_enemies, [](const auto* enemy) {
        return !enemy || enemy->is_defeated();
    });
}

int GameLevel::remaining_enemy_count() const noexcept
{
    return static_cast<int>(std::ranges::count_if(_enemies, [](const auto* enemy) {
        return enemy && !enemy->is_defeated();
    }));
}

int GameLevel::total_enemy_hit_points() const noexcept
{
    int total = 0;
    for (const auto* enemy : _enemies)
        if (enemy) total += enemy->hit_points();
    return total;
}
}
