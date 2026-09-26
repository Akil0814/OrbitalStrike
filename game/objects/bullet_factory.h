#pragma once

#include "bullet.h"

#include <functional>

namespace elysia::scene { class Scene; }

namespace game::objects
{
struct BulletSpawnRequest
{
    elysia::core::Vector2 position{}, velocity{};
    int damage = 1;
    std::function<void(elysia::physics::ColliderId, int)> on_hit;
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
