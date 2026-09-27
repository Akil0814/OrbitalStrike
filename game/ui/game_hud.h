#pragma once

#include "engine/input/input_types.h"

#include <string>

namespace elysia::scene { class Scene; }

namespace elysia::ui
{ 
    class UiPanel;
    class UiLabel;
    class UiButton;
    class UiBar;
    class UiWindow;
    class UiConfirmationDialog;
}

namespace game::ui
{
struct GameHudModel
{
    bool victory = false;
    bool projectile_in_flight = false;
    bool resolving = false;
    elysia::input::InputDevice input_device = elysia::input::InputDevice::Keyboard;

    float power = 0.0f;//0 to 1;
    float enemy_charge_level = 0.0f;

    int flagship_hit_points = 0;
    int flagship_maximum_hit_points = 0;
    bool flagship_shield_active = false;
    int living_escorts = 0;
};

class GameHud final
{
public:
    void build(elysia::scene::Scene& scene);
    void update(const GameHudModel& model);
    void clear() noexcept;
    [[nodiscard]] bool is_built() const noexcept { return _panel != nullptr; }

private:
    elysia::ui::UiPanel* _panel = nullptr;
    elysia::ui::UiWindow* _hud_window = nullptr;

    elysia::ui::UiBar* _power_bar = nullptr;
    elysia::ui::UiBar* _enemy_charge_level = nullptr;

    elysia::ui::UiLabel* _current_status_lable = nullptr;
    elysia::ui::UiLabel* _lable = nullptr;

    elysia::ui::UiButton* _pause_button = nullptr;
    elysia::ui::UiConfirmationDialog* _exit_confirmation = nullptr;
};
}
