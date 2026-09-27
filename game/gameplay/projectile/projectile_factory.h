#pragma once

#include "projectile.h"

#include <functional>

namespace elysia::scene { class Scene; }

namespace game::projectile
{
struct ProjectileSpawnRequest
{
    elysia::core::Vector2 position{}, velocity{};
    elysia::core::Rect despawn_bounds{0.0f, 0.0f, 1600.0f, 1000.0f};
    ProjectileDefinition definition{};
    std::function<ProjectileImpactResolution(const ProjectileImpact&)> on_impact;
    std::function<void(ProjectileEndReason)> on_finished;
};

class ProjectileFactory final
{
public:
    explicit ProjectileFactory(elysia::scene::Scene& scene) noexcept : _scene(scene) {}
    [[nodiscard]] Projectile* spawn(ProjectileSpawnRequest request) const;

private:
    elysia::scene::Scene& _scene;
};
}
