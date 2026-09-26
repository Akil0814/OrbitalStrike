#pragma once

#include "bullet.h"

#include <functional>

namespace elysia::scene { class Scene; }

namespace game::objects
{
struct BulletSpawnRequest
{
    elysia::core::Vector2 position{}, velocity{};
    elysia::core::Rect flight_bounds{0.0f, 0.0f, 1600.0f, 1000.0f};
    int damage = 1;
    std::function<ProjectileCollisionResult(const ProjectileHitContext&)> on_hit;
    std::function<void(BulletEndReason)> on_finished;
};

class BulletFactory final
{
public:
    explicit BulletFactory(elysia::scene::Scene& scene) noexcept : _scene(scene) {}
    [[nodiscard]] Bullet* spawn(BulletSpawnRequest request) const;

private:
    elysia::scene::Scene& _scene;
};
} // namespace game::objects
