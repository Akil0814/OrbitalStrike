#pragma once

#include "game_level_definition.h"

#include <span>
#include <string_view>

namespace game::level
{
struct GameLevelEntry
{
    GameLevelId id;
    std::string_view title;
};

class GameLevelCatalog final
{
public:
    [[nodiscard]] static std::span<const GameLevelEntry> entries() noexcept;
    [[nodiscard]] static const GameLevelDefinition& get(GameLevelId level_id);
};
}
