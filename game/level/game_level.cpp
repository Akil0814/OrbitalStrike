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
    _ships.reserve(definition.ships.size());
    _anomalies.reserve(definition.anomalies.size());
    _interactors.reserve(1 + definition.ships.size() + definition.anomalies.size());
    _background = scene.create_and_add_object<game::objects::ArenaBackdrop>(
        definition.map.backdrop_bounds, definition.map.starfield);
    _moon_cell = scene.create_and_add_object<game::objects::MoonCell>(definition.moon_cell);
    if (_moon_cell) _interactors.push_back(_moon_cell);

    for (const auto& ship_config : definition.ships)
    {
        auto* ship = scene.create_and_add_object<game::objects::EnemyShip>(ship_config);
        if (!ship) continue;
        _ships.push_back(ship);
        _interactors.push_back(ship);
    }
    for (const auto& anomaly_config : definition.anomalies)
    {
        auto* anomaly = scene.create_and_add_object<game::objects::SpaceAnomaly>(anomaly_config);
        if (!anomaly) continue;
        _anomalies.push_back(anomaly);
        _interactors.push_back(anomaly);
    }

    if (!_background || !_moon_cell || _ships.size() != definition.ships.size()
        || _anomalies.size() != definition.anomalies.size())
    {
        clear();
        throw std::runtime_error("GameLevel failed to create its scene objects.");
    }

    try { _fleet.configure(_ships); }
    catch (...) { clear(); throw; }
}

void GameLevel::clear() noexcept
{
    const auto destroy = [](elysia::core::SceneObject* object) {
        if (object && !object->is_destroyed()) object->destroy();
    };
    _fleet.clear();
    for (auto* anomaly : _anomalies) destroy(anomaly);
    for (auto* ship : _ships) destroy(ship);
    destroy(_moon_cell);
    destroy(_background);
    _interactors.clear();
    _anomalies.clear();
    _ships.clear();
    _moon_cell = nullptr;
    _background = nullptr;
    _definition = nullptr;
}

game::objects::ProjectileCollisionResult GameLevel::resolve_projectile_hit(
    const game::objects::ProjectileHitContext& hit)
{
    if (auto* ship = _fleet.find_ship(hit.target_collider))
        return _fleet.resolve_projectile_hit(*ship, hit);
    if (auto* interactor = find_interactor(hit.target_collider))
        return interactor->on_projectile_hit(hit);
    return {};
}

void GameLevel::set_backdrop_visible_bounds(elysia::core::Rect bounds) noexcept
{
    if (_background) _background->set_visible_bounds(bounds);
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
}
