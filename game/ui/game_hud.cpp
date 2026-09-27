#include "game_hud.h"

#include "engine/scene/scene.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_bar.h"
#include "engine/ui/containers/ui_panel.h"

#include <cmath>
#include <stdexcept>

namespace game::ui
{
void GameHud::build(elysia::scene::Scene& scene)
{
    if (_panel != nullptr)
        return;

    _panel = scene.create_and_add_object<elysia::ui::UiPanel>(elysia::core::Rect{0,0,1280,720});
    auto power_bar = std::make_unique<elysia::ui::UiBar>(
        elysia::core::Vector2{ 0,0 }, elysia::core::Vector2{ 50,300 });
    power_bar->set_fill_direction(elysia::ui::BarFillDirection::BottomToTop);
    power_bar->set_range(0.0f, 1.0f);
    power_bar->set_ratio(0.5f);
    _panel->add_child(std::move(power_bar), elysia::ui::UiLayoutChildOptions
        {
            ._anchor = elysia::ui::UiLayoutAnchor::CenterLeft
        });

}

void GameHud::update(const GameHudModel& model)
{

}

void GameHud::clear() noexcept
{

}
}
