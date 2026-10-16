#pragma once

#include <cstdint>
#include "engine/tools/random_generator.h"

namespace game::presentation
{
struct StarfieldConfig
{
    std::uint64_t seed = elysia::tools::RandomGenerator::TestSeedTwo;
    float stars_per_million_units = 35.0f;
    int minimum_stars = 180;
    int maximum_stars = 5000;
    float twinkle_ratio = 0.3f;
};
}
