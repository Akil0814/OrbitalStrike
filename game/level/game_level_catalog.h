#pragma once

#include "game_level_definition.h"

namespace game::level
{
class GameLevelCatalog final
{
public:
    [[nodiscard]] static const GameLevelDefinition& get(GameLevelId level_id);
};
}
