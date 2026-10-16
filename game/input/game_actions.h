#pragma once

#include "engine/input/action/input_action_map.h"

namespace game::input
{
namespace actions
{
inline const elysia::input::InputActionId CameraPan{"game.camera_pan"};
inline const elysia::input::InputActionId CameraPanStick{"game.camera_pan_stick"};
inline const elysia::input::InputActionId AimStick{"game.aim_stick"};
inline const elysia::input::InputActionId PowerAdjust{"game.power_adjust"};
inline const elysia::input::InputActionId ZoomContinuous{"game.zoom_continuous"};
inline const elysia::input::InputActionId ZoomWheel{"game.zoom_wheel"};
inline const elysia::input::InputActionId Fire{"game.fire"};
inline const elysia::input::InputActionId Restart{"game.restart"};
}

[[nodiscard]] elysia::input::InputActionMap make_game_actions();
}
