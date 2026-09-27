#include "game_level.h"

#include "../gameplay/anomaly/black_hole_anomaly.h"
#include "../gameplay/anomaly/wormhole_portal.h"
#include "../presentation/backdrop/arena_backdrop.h"
#include "engine/scene/scene.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace game::level
{
void GameLevel::build(elysia::scene::Scene& scene, const GameLevelDefinition& definition)
{
    if (is_built()) throw std::logic_error("GameLevel is already built.");

    _definition = &definition;
    _ships.reserve(definition.ships.size());
    _anomalies.reserve(definition.anomalies.size());
    _force_sources.reserve(1 + definition.ships.size() + definition.anomalies.size());
    _impact_targets.reserve(1 + definition.ships.size());
    _background = scene.create_and_add_object<game::presentation::ArenaBackdrop>(
        definition.map.backdrop_bounds, definition.map.starfield);
    _moon_cell = scene.create_and_add_object<game::launcher::MoonCell>(definition.moon_cell);
    if (_moon_cell)
    {
        _force_sources.push_back(_moon_cell);
        _impact_targets.push_back(_moon_cell);
    }

    for (const auto& ship_config : definition.ships)
    {
        auto* ship = scene.create_and_add_object<game::fleet::EnemyShip>(ship_config);
        if (!ship) continue;
        _ships.push_back(ship);
        _force_sources.push_back(ship);
        _impact_targets.push_back(ship);
    }
    const auto register_anomaly = [this](game::anomaly::SpaceAnomaly* anomaly)
    {
        if (!anomaly) return false;
        _anomalies.push_back(anomaly);
        if (auto* source = dynamic_cast<game::projectile::ProjectileForceSource*>(anomaly))
            _force_sources.push_back(source);
        if (auto* target = dynamic_cast<game::projectile::ProjectileImpactTarget*>(anomaly))
            _impact_targets.push_back(target);
        return true;
    };

    bool anomalies_created = true;
    for (const auto& anomaly_definition : definition.anomalies)
        std::visit([&](const auto& anomaly_config) {
            using Config = std::decay_t<decltype(anomaly_config)>;
            if constexpr (std::is_same_v<Config, game::anomaly::RadialFieldAnomalyConfig>)
            {
                anomalies_created &= register_anomaly(
                    scene.create_and_add_object<game::anomaly::RadialFieldAnomaly>(anomaly_config));
            }
            else if constexpr (std::is_same_v<Config, game::anomaly::BlackHoleConfig>)
            {
                anomalies_created &= register_anomaly(
                    scene.create_and_add_object<game::anomaly::BlackHoleAnomaly>(anomaly_config));
            }
            else if constexpr (std::is_same_v<Config, game::anomaly::WormholePairConfig>)
            {
                anomalies_created &= register_anomaly(
                    scene.create_and_add_object<game::anomaly::WormholePortal>(
                        game::anomaly::WormholePortalConfig{
                            .center = anomaly_config.first_center,
                            .destination_center = anomaly_config.second_center,
                            .portal_radius = anomaly_config.portal_radius,
                            .exit_offset = anomaly_config.exit_offset,
                            .color = anomaly_config.first_color}));
                anomalies_created &= register_anomaly(
                    scene.create_and_add_object<game::anomaly::WormholePortal>(
                        game::anomaly::WormholePortalConfig{
                            .center = anomaly_config.second_center,
                            .destination_center = anomaly_config.first_center,
                            .portal_radius = anomaly_config.portal_radius,
                            .exit_offset = anomaly_config.exit_offset,
                            .color = anomaly_config.second_color}));
            }
        }, anomaly_definition);

    if (!_background || !_moon_cell || _ships.size() != definition.ships.size()
        || !anomalies_created)
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
    _impact_targets.clear();
    _force_sources.clear();
    _anomalies.clear();
    _ships.clear();
    _moon_cell = nullptr;
    _background = nullptr;
    _definition = nullptr;
}

game::projectile::ProjectileImpactResolution GameLevel::resolve_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    if (auto* ship = _fleet.find_ship(impact.target_collider))
        return _fleet.resolve_projectile_impact(*ship, impact);
    if (auto* target = find_impact_target(impact.target_collider))
        return target->resolve_projectile_impact(impact);
    return {};
}

void GameLevel::set_backdrop_visible_bounds(elysia::core::Rect bounds) noexcept
{
    if (_background) _background->set_visible_bounds(bounds);
}

game::projectile::ProjectileImpactTarget* GameLevel::find_impact_target(
    elysia::physics::ColliderId collider) const noexcept
{
    if (collider == elysia::physics::InvalidColliderId) return nullptr;
    const auto found = std::ranges::find_if(_impact_targets, [collider](const auto* target) {
        return target && target->collider_id() == collider;
    });
    return found == _impact_targets.end() ? nullptr : *found;
}
}
