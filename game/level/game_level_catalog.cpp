#include "game_level_catalog.h"

#include <stdexcept>

namespace game::level
{
const GameLevelDefinition& GameLevelCatalog::get(GameLevelId level_id)
{
    static const GameLevelDefinition prototype{
        .activity_bounds = {0.0f, 0.0f, 1600.0f, 1000.0f},
        .initial_zoom = 0.8f,
        .initial_power = 700.0f,
        .player = {
            .center = {230.0f, 720.0f},
            .radius = 66.0f,
            .faction = game::objects::CelestialFaction::Player,
            .hit_points = 1,
            .gravity_strength = 16.0f,
            .color = {63, 145, 255}},
        .enemy = {
            .center = {1320.0f, 240.0f},
            .radius = 74.0f,
            .faction = game::objects::CelestialFaction::Enemy,
            .hit_points = 3,
            .gravity_strength = 20.0f,
            .color = {218, 70, 86}},
        .neutral_bodies = {{
            .center = {760.0f, 570.0f},
            .radius = 100.0f,
            .faction = game::objects::CelestialFaction::Neutral,
            .hit_points = 1,
            .gravity_strength = 35.0f,
            .color = {126, 102, 176}}},
        .gravity = {
            .maximum_range = 520.0f,
            .minimum_distance = 140.0f,
            .maximum_force = 80.0f}};

    switch (level_id)
    {
    case GameLevelId::Prototype:
        return prototype;
    }
    throw std::invalid_argument("Unknown GameLevelId.");
}
}
