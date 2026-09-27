#include "game_hud.h"

#include "engine/scene/scene.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"

#include <cmath>
#include <stdexcept>

namespace game::ui
{
void GameHud::build(elysia::scene::Scene& scene)
{
    if (_label) throw std::logic_error("GameHud is already built.");
    _label = scene.create_and_add_object<elysia::ui::UiLabel>(
        elysia::core::Rect{24.0f, 20.0f, 600.0f, 45.0f});
    if (!_label) throw std::runtime_error("GameHud failed to create its label.");
    _label->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
    _label->set_text_content(elysia::ui::ui_raw_text(""));
    _last_text.clear();
}

void GameHud::update(const GameHudModel& model)
{
    if (!_label) return;
    const bool gamepad = model.input_device == elysia::input::InputDevice::Gamepad;
    std::string text;
    if (model.victory)
        text = gamepad ? "Victory! Press Y to restart" : "Victory! Press R to restart";
    else if (model.resolving)
        text = gamepad ? "Observing result | LB/RB zoom" : "Observing result | Mouse wheel zoom";
    else if (model.projectile_in_flight)
        text = gamepad ? "Projectile in flight | LB/RB zoom" : "Projectile in flight | Mouse wheel zoom";
    else
    {
        const std::string status = " | Flagship: "
            + std::to_string(model.flagship_hit_points) + "/"
            + std::to_string(model.flagship_maximum_hit_points)
            + " | Shield: " + (model.flagship_shield_active ? "ONLINE" : "OFFLINE")
            + " | Escorts: " + std::to_string(model.living_escorts);
        if (gamepad)
            text = "LS move | RS aim | LT/RT power: "
                + std::to_string(static_cast<int>(std::lround(model.power)))
                + " | LB/RB zoom | A fire" + status;
        else
            text = "WASD move | Mouse aim | Q/E power: "
                + std::to_string(static_cast<int>(std::lround(model.power)))
                + " | Wheel zoom | Left click fire" + status;
    }

    if (text == _last_text) return;
    _last_text = text;
    _label->set_text_content(elysia::ui::ui_raw_text(_last_text));
}

void GameHud::clear() noexcept
{
    if (_label && !_label->is_destroyed()) _label->destroy();
    _label = nullptr;
    _last_text.clear();
}
}
