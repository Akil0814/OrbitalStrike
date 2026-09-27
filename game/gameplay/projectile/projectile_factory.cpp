#include "projectile_factory.h"

#include "engine/resources/resource_service.h"
#include "engine/scene/scene.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace game::projectile
{
Projectile* ProjectileFactory::spawn(ProjectileSpawnRequest request) const
{
    SDL_Texture* texture = ELYSIA_RESOURCES->find_texture(request.definition.texture_key);
    if (!texture)
        throw std::runtime_error(
            "Required projectile texture is unavailable: "
            + std::string(request.definition.texture_key));

    ProjectileConfig config;
    config.position = request.position;
    config.velocity = request.velocity;
    config.despawn_bounds = request.despawn_bounds;
    config.definition = request.definition;
    config.texture = texture;
    config.on_impact = std::move(request.on_impact);
    config.on_finished = std::move(request.on_finished);
    return _scene.add_object(std::make_unique<Projectile>(std::move(config)));
}
}
