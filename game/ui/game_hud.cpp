#include "game_hud.h"

#include "engine/scene/scene.h"
#include "engine/typography/font_settings.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/layout/ui_layout_types.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_bar.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace game::ui
{
namespace
{
constexpr elysia::core::Color kHudSurface{8, 18, 38, 218};
constexpr elysia::core::Color kHudBorder{94, 172, 230, 220};
constexpr elysia::core::Color kPowerFill{255, 190, 76, 255};
constexpr elysia::core::Color kHealthFill{226, 72, 88, 255};
constexpr elysia::core::Color kShieldFill{84, 190, 255, 255};

elysia::ui::UiLayoutChildOptions anchored(
    elysia::ui::UiLayoutAnchor anchor,
    elysia::core::Vector2 size,
    elysia::ui::UiLayoutMargin margin = {})
{
    return {
        ._anchor = anchor,
        ._margin = margin,
        ._size_override = size,
        ._use_size_override = true};
}

std::unique_ptr<elysia::ui::UiLabel> make_hud_label(
    elysia::core::Vector2 size,
    elysia::typography::UiTypographyRole typography,
    elysia::ui::TextHorizontalAlign horizontal_align)
{
    auto label = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Vector2{}, size);
    label->set_typography_role(typography);
    label->set_horizontal_align(horizontal_align);
    label->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    label->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
    label->set_padding(10);
    label->set_style_overrides({
        .corner_radius = 10.0f,
        .background = kHudSurface,
        .draw_background = true});
    return label;
}

float normalized(float value, float minimum, float maximum) noexcept
{
    if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum)
        || maximum <= minimum)
        return 0.0f;
    return std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
}

std::string status_text(const GameHudModel& model)
{
    if (model.victory) return "VICTORY";
    if (model.resolving) return "IMPACT CONFIRMED";
    if (model.projectile_in_flight) return "PROJECTILE IN FLIGHT";
    return "AIMING";
}

std::string hint_text(const GameHudModel& model)
{
    const bool gamepad = model.input_device == elysia::input::InputDevice::Gamepad;
    if (model.victory)
        return gamepad ? "Y: restart mission" : "R: restart mission";
    return gamepad
        ? "Right stick: aim  |  Triggers: power  |  A: fire  |  Left stick: pan"
        : "Mouse: aim / fire  |  Q / E: power  |  WASD: pan  |  Wheel: zoom";
}
}

void GameHud::build(elysia::scene::Scene& scene, elysia::core::Vector2 viewport_size)
{
    if (_root && !_root->is_destroyed()) return;
    clear();

    viewport_size.x = std::max(1.0f, viewport_size.x);
    viewport_size.y = std::max(1.0f, viewport_size.y);
    _root = scene.create_and_add_object<elysia::ui::UiPanel>(
        elysia::core::Vector2{}, viewport_size, 100);
    if (!_root) throw std::runtime_error("GameHud failed to create its root panel.");
    _root->set_style_overrides({.draw_background = false, .draw_border = false});

    auto status = make_hud_label(
        {360.0f, 48.0f}, elysia::typography::UiTypographyRole::Heading,
        elysia::ui::TextHorizontalAlign::Center);
    _status_label = static_cast<elysia::ui::UiLabel*>(_root->add_child(
        std::move(status), anchored(
            elysia::ui::UiLayoutAnchor::TopCenter, {360.0f, 48.0f},
            {.top = 20.0f})));

    auto objective = make_hud_label(
        {320.0f, 42.0f}, elysia::typography::UiTypographyRole::Heading,
        elysia::ui::TextHorizontalAlign::Center);
    _objective_label = static_cast<elysia::ui::UiLabel*>(_root->add_child(
        std::move(objective), anchored(
            elysia::ui::UiLayoutAnchor::TopRight, {320.0f, 42.0f},
            {.top = 20.0f, .right = 24.0f})));

    auto health = std::make_unique<elysia::ui::UiBar>(
        elysia::core::Vector2{}, elysia::core::Vector2{320.0f, 24.0f});
    health->set_range(0.0f, 1.0f);
    health->set_padding(3);
    health->set_visual_role(elysia::ui::UiBarVisualRole::Progress);
    health->set_style_overrides({
        .corner_radius = 8.0f,
        .background = kHudSurface,
        .fill = kHealthFill,
        .border = kHudBorder,
        .draw_border = true});
    _flagship_health_bar = static_cast<elysia::ui::UiBar*>(_root->add_child(
        std::move(health), anchored(
            elysia::ui::UiLayoutAnchor::TopRight, {320.0f, 24.0f},
            {.top = 70.0f, .right = 24.0f})));

    auto fleet = make_hud_label(
        {320.0f, 38.0f}, elysia::typography::UiTypographyRole::LabelMuted,
        elysia::ui::TextHorizontalAlign::Center);
    _fleet_label = static_cast<elysia::ui::UiLabel*>(_root->add_child(
        std::move(fleet), anchored(
            elysia::ui::UiLayoutAnchor::TopRight, {320.0f, 38.0f},
            {.top = 102.0f, .right = 24.0f})));

    auto power = std::make_unique<elysia::ui::UiBar>(
        elysia::core::Vector2{}, elysia::core::Vector2{34.0f, 250.0f});
    power->set_fill_direction(elysia::ui::BarFillDirection::BottomToTop);
    power->set_range(0.0f, 1.0f);
    power->set_padding(3);
    power->set_visual_role(elysia::ui::UiBarVisualRole::Progress);
    power->set_style_overrides({
        .corner_radius = 10.0f,
        .background = kHudSurface,
        .fill = kPowerFill,
        .border = kHudBorder,
        .draw_border = true});
    _power_bar = static_cast<elysia::ui::UiBar*>(_root->add_child(
        std::move(power), anchored(
            elysia::ui::UiLayoutAnchor::CenterLeft, {34.0f, 250.0f},
            {.left = 30.0f})));

    auto power_label = make_hud_label(
        {132.0f, 44.0f}, elysia::typography::UiTypographyRole::Number,
        elysia::ui::TextHorizontalAlign::Center);
    _power_label = static_cast<elysia::ui::UiLabel*>(_root->add_child(
        std::move(power_label), anchored(
            elysia::ui::UiLayoutAnchor::CenterLeft, {132.0f, 44.0f},
            {.left = 76.0f})));

    auto hint = make_hud_label(
        {720.0f, 40.0f}, elysia::typography::UiTypographyRole::ButtonCompact,
        elysia::ui::TextHorizontalAlign::Center);
    _hint_label = static_cast<elysia::ui::UiLabel*>(_root->add_child(
        std::move(hint), anchored(
            elysia::ui::UiLayoutAnchor::BottomCenter, {720.0f, 40.0f},
            {.bottom = 18.0f})));

    if (!_status_label || !_objective_label || !_flagship_health_bar || !_fleet_label
        || !_power_bar || !_power_label || !_hint_label)
    {
        clear();
        throw std::runtime_error("GameHud failed to create one or more widgets.");
    }
}

void GameHud::update(const GameHudModel& model)
{
    if (!is_built()) return;

    _power_bar->set_ratio(normalized(model.power, model.minimum_power, model.maximum_power));
    _power_label->set_text_content(elysia::ui::ui_raw_text(
        "POWER " + std::to_string(static_cast<int>(std::lround(model.power)))));

    const float health_ratio = model.flagship_maximum_hit_points > 0
        ? static_cast<float>(model.flagship_hit_points)
            / static_cast<float>(model.flagship_maximum_hit_points)
        : 0.0f;
    _flagship_health_bar->set_ratio(std::clamp(health_ratio, 0.0f, 1.0f));
    _flagship_health_bar->set_style_overrides({
        .corner_radius = 8.0f,
        .background = kHudSurface,
        .fill = model.flagship_shield_active ? kShieldFill : kHealthFill,
        .border = kHudBorder,
        .draw_border = true});
    _objective_label->set_text_content(elysia::ui::ui_raw_text(
        "FLAGSHIP  " + std::to_string(std::max(0, model.flagship_hit_points))
        + " / " + std::to_string(std::max(0, model.flagship_maximum_hit_points))));
    _fleet_label->set_text_content(elysia::ui::ui_raw_text(
        std::string(model.flagship_shield_active ? "SHIELD ACTIVE" : "SHIELD DOWN")
        + "  |  ESCORTS " + std::to_string(std::max(0, model.living_escorts))));
    _status_label->set_text_content(elysia::ui::ui_raw_text(status_text(model)));
    _hint_label->set_text_content(elysia::ui::ui_raw_text(hint_text(model)));
}

void GameHud::clear() noexcept
{
    if (_root && !_root->is_destroyed()) _root->destroy();
    _root = nullptr;
    _power_bar = nullptr;
    _flagship_health_bar = nullptr;
    _power_label = nullptr;
    _status_label = nullptr;
    _objective_label = nullptr;
    _fleet_label = nullptr;
    _hint_label = nullptr;
}
}
