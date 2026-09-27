#pragma once

namespace game::combat
{
struct DamageSpec
{
    int amount = 1;
};

struct DamageResult
{
    int requested = 0;
    int applied = 0;
    bool blocked = false;
    bool defeated = false;
};
}
