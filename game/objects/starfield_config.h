#pragma once

#include <cstdint>

namespace game::objects
{
struct StarfieldConfig
{
    std::uint64_t seed = 0x4F52424954414C53ULL;
    float stars_per_million_units = 35.0f;
    int minimum_stars = 180;
    int maximum_stars = 5000;
    float twinkle_ratio = 0.3f;
};
}
