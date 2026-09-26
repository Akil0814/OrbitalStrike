#include "level_select_scene.h"

#include "../../engine/tools/logger.h"

#include "../../engine/ui/composites/ui_confirmation_dialog.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/widgets/image/ui_image.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/containers/ui_grid_container.h"
#include "../../engine/ui/layout/ui_layout_types.h"

namespace game::scene
{

    void LevelSelectScene::on_update(double delta)
    {
        elysia::scene::Scene::on_update(delta);
    }

    void LevelSelectScene::on_enter(const elysia::scene::ScenePayload& payload)
    {
        (void)payload;

        ELYSIA_LOG_DEBUG("LevelSelectScene","on_enter");

        if (_has_entered)
            return;

        if (!_main_window || _main_window->is_destroyed())
            build_window_button();

        _has_entered = true;
    }

    void LevelSelectScene::on_exit()
    {
    }

    void LevelSelectScene::reset()
    {
        _has_entered = false;
    }

    void LevelSelectScene::build_window_button()
    {
        if (_main_window && _main_window->is_destroyed())
            return;

        //create window
        _main_window = Scene::create_and_add_object<elysia::ui::UiWindow>(
            elysia::core::Rect{ 0.0f,0.0f,
                static_cast<float>(runtime_context().logical_width()),
                static_cast<float>(runtime_context().logical_height()) },
            10);

        if (!_main_window)
            return;

        //create the inner button container
        std::unique_ptr<elysia::ui::UiListContainer> ui_list =
            std::make_unique<elysia::ui::UiListContainer>(
                elysia::core::Rect{ 0, 0, 300, 260 });

        //Title
        std::unique_ptr<elysia::ui::UiLabel> ui_label =
            std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{ 0,0,600,120 }, 0, elysia::ui::ui_text_key("menu_scene.project_name"));
        ui_label->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
        ui_label->set_typography_role(
            elysia::typography::UiTypographyRole::Title);
        ui_label->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ScaleToFit);
        ui_label->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
        ui_label->set_vertical_align(elysia::ui::TextVerticalAlign::Center);

        ui_list->add_back(std::move(ui_label));

        std::unique_ptr <elysia::ui::UiGridContainer> ui_grid =
            std::make_unique<elysia::ui::UiGridContainer>(
                elysia::core::Rect{ 0, 0, 300, 260 });

        constexpr int button_wide = 200;
        constexpr int button_hight = 200;
        const elysia::ui::UiButtonSounds menu_button_sounds{
            .press = "system.button_click_down",
            .click = "system.button_click_up" };

    }
}
