#include "bullet_factory.h"

#include "engine/scene/scene.h"

#include <memory>

namespace game::objects
{
Bullet* BulletFactory::spawn(BulletSpawnRequest request) const
{
    BulletConfig config;
    config.position = request.position;
    config.velocity = request.velocity;
    config.despawn_bounds = request.despawn_bounds;
    config.damage = request.damage;
    config.lifetime_seconds = request.lifetime_seconds;
    config.on_hit = std::move(request.on_hit);
    config.on_finished = std::move(request.on_finished);
    return _scene.add_object(std::make_unique<Bullet>(std::move(config)));
}
} // namespace game::objects
