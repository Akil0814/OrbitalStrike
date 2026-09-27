#pragma once

#include "engine/core/geometry/vector2.h"
#include "engine/input/input_types.h"

namespace elysia::scene { class Scene; }

namespace elysia::ui
{
class UiPanel;
class UiLabel;
class UiBar;
}

namespace game::ui
{
struct GameHudModel
{
    bool victory = false;
    bool defeat = false;
    bool flagship_warning = false;
    bool flagship_firing = false;
    bool projectile_in_flight = false;
    bool resolving = false;
    elysia::input::InputDevice input_device = elysia::input::InputDevice::Keyboard;

    float power = 0.0f;
    float minimum_power = 0.0f;
    float maximum_power = 1.0f;

    int flagship_hit_points = 0;
    int flagship_maximum_hit_points = 0;
    bool flagship_shield_active = false;
    int living_escorts = 0;
    int completed_rounds = 0;
    int maximum_rounds = 10;
};

class GameHud final
{
public:
    void build(elysia::scene::Scene& scene, elysia::core::Vector2 viewport_size);
    void update(const GameHudModel& model);
    void clear() noexcept;
    [[nodiscard]] bool is_built() const noexcept { return _root != nullptr; }

private:
    elysia::ui::UiPanel* _root = nullptr;
    elysia::ui::UiBar* _power_bar = nullptr;
    elysia::ui::UiBar* _flagship_health_bar = nullptr;
    elysia::ui::UiBar* _charge_bar = nullptr;
    elysia::ui::UiLabel* _power_label = nullptr;
    elysia::ui::UiLabel* _status_label = nullptr;
    elysia::ui::UiLabel* _objective_label = nullptr;
    elysia::ui::UiLabel* _fleet_label = nullptr;
    elysia::ui::UiLabel* _charge_label = nullptr;
    elysia::ui::UiLabel* _hint_label = nullptr;
};
}
