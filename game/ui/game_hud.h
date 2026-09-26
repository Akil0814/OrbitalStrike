#pragma once

#include "engine/input/input_types.h"

#include <string>

namespace elysia::scene { class Scene; }
namespace elysia::ui { class UiLabel; }

namespace game::ui
{
struct GameHudModel
{
    bool victory = false;
    bool projectile_in_flight = false;
    bool resolving = false;
    elysia::input::InputDevice input_device = elysia::input::InputDevice::Keyboard;
    float power = 0.0f;
    int remaining_enemies = 0;
    int total_enemy_hit_points = 0;
};

class GameHud final
{
public:
    void build(elysia::scene::Scene& scene);
    void update(const GameHudModel& model);
    void clear() noexcept;
    [[nodiscard]] bool is_built() const noexcept { return _label != nullptr; }

private:
    elysia::ui::UiLabel* _label = nullptr;
    std::string _last_text;
};
}
