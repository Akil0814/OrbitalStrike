#include "bullet_factory.h"

#include "../resources/game_texture_keys.h"
#include "engine/scene/scene.h"
#include "engine/resources/resource_service.h"

#include <memory>
#include <stdexcept>

namespace game::objects
{
Bullet* BulletFactory::spawn(BulletSpawnRequest request) const
{
    SDL_Texture* projectile_texture = ELYSIA_RESOURCES->find_texture(
        game::resources::texture_keys::Projectile);
    if (!projectile_texture)
        throw std::runtime_error("Required texture is unavailable: projectile");

    BulletConfig config;
    config.position = request.position;
    config.velocity = request.velocity;
    config.despawn_bounds = request.despawn_bounds;
    config.texture = projectile_texture;
    config.damage = request.damage;
    config.lifetime_seconds = request.lifetime_seconds;
    config.on_hit = std::move(request.on_hit);
    config.on_finished = std::move(request.on_finished);
    return _scene.add_object(std::make_unique<Bullet>(std::move(config)));
}
} // namespace game::objects
