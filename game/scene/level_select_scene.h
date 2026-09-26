#pragma once

#include "../../engine/scene/scene.h"

#include "../../engine/ui/window/ui_window.h"

namespace game::scene
{
	class LevelSelectScene final : public elysia::scene::Scene
	{
	public:
        LevelSelectScene() = default;
        ~LevelSelectScene() override = default;

        void on_update(double delta) override;

        void on_enter(const elysia::scene::ScenePayload& payload) override;
        void on_exit() override;
        void reset() override;

    private:

        void build_window_button();

    private:
        bool _has_entered = false;
        elysia::ui::UiWindow* _main_window = nullptr;
	};

}
