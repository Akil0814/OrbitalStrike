#include "game/gameplay/fleet/enemy_fleet.h"
#include "game/gameplay/anomaly/black_hole_anomaly.h"
#include "game/gameplay/anomaly/wormhole_portal.h"
#include "game/gameplay/projectile/projectile.h"
#include "game/gameplay/projectile/projectile_interaction.h"
#include "game/gameplay/session/round_controller.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

void expect_near(float actual, float expected, std::string_view message)
{
    expect(std::fabs(actual - expected) <= 0.0001f, message);
}

void test_damage()
{
    game::fleet::EnemyShip ship({.hit_points = 3});

    const auto normal = ship.receive_damage({.amount = 1});
    expect(normal.requested == 1 && normal.applied == 1, "normal damage is applied");
    expect(!normal.blocked && !normal.defeated && ship.hit_points() == 2,
           "normal damage preserves a living target");

    const auto zero = ship.receive_damage({.amount = 0});
    const auto negative = ship.receive_damage({.amount = -4});
    expect(zero.applied == 0 && negative.applied == 0 && ship.hit_points() == 2,
           "non-positive damage is ignored");

    const auto lethal = ship.receive_damage({.amount = 5});
    expect(lethal.applied == 2 && lethal.defeated && ship.hit_points() == 0,
           "overkill damage is clamped to remaining hit points");
    const auto repeated = ship.receive_damage({.amount = 1});
    expect(repeated.applied == 0 && repeated.defeated,
           "damage does not apply after defeat");
}

void test_fleet_shield()
{
    game::fleet::EnemyShip flagship({
        .role = game::fleet::ShipRole::Flagship,
        .hit_points = 5});
    game::fleet::EnemyShip projector({
        .role = game::fleet::ShipRole::Escort,
        .ability = game::fleet::ShipAbility::ShieldProjector,
        .hit_points = 1});
    std::array<game::fleet::EnemyShip*, 2> ships{&flagship, &projector};
    game::fleet::EnemyFleet fleet;
    fleet.configure(ships);

    const game::projectile::ProjectileImpact impact{
        .projectile_velocity = {10.0f, 0.0f},
        .damage = {.amount = 1}};
    const auto blocked = fleet.resolve_projectile_impact(flagship, impact);
    expect(blocked.disposition == game::projectile::ProjectileDisposition::Reflect,
           "active fleet shield reflects a flagship impact");
    expect_near(blocked.restitution, 0.85f, "fleet shield uses the existing restitution");
    expect(blocked.damage.blocked && blocked.damage.applied == 0
               && flagship.hit_points() == 5,
           "active fleet shield blocks flagship damage");

    const auto projector_hit = fleet.resolve_projectile_impact(projector, impact);
    expect(projector_hit.damage.defeated && !fleet.flagship_shield_active(),
           "defeating the projector disables the fleet shield");
    const auto unblocked = fleet.resolve_projectile_impact(flagship, impact);
    expect(!unblocked.damage.blocked && unblocked.damage.applied == 1
               && flagship.hit_points() == 4,
           "flagship takes damage after the projector is defeated");
}

void test_projectile_motion()
{
    using game::projectile::ProjectileDisposition;
    using game::projectile::ProjectileImpactResolution;

    const auto continued = game::projectile::resolve_projectile_motion(
        ProjectileImpactResolution{.disposition = ProjectileDisposition::Continue},
        {4.0f, -2.0f}, {0.0f, 1.0f});
    expect(!continued.should_finish && continued.velocity == elysia::core::Vector2{4.0f, -2.0f},
           "continue preserves projectile velocity");

    const auto reflected = game::projectile::resolve_projectile_motion(
        ProjectileImpactResolution{
            .disposition = ProjectileDisposition::Reflect, .restitution = 0.5f},
        {4.0f, -2.0f}, {0.0f, 1.0f});
    expect(!reflected.should_finish, "reflection keeps the projectile active");
    expect_near(reflected.velocity.x, 2.0f, "reflection scales tangential velocity");
    expect_near(reflected.velocity.y, 1.0f, "reflection reverses normal velocity");

    const auto destroyed = game::projectile::resolve_projectile_motion(
        ProjectileImpactResolution{.disposition = ProjectileDisposition::Destroy},
        {4.0f, -2.0f}, {0.0f, 1.0f});
    expect(destroyed.should_finish, "destroy marks the projectile as finished");

    const auto teleported = game::projectile::resolve_projectile_motion(
        ProjectileImpactResolution{
            .disposition = ProjectileDisposition::Teleport,
            .teleport_position = elysia::core::Vector2{120.0f, 80.0f}},
        {4.0f, -2.0f}, {});
    expect(!teleported.should_finish
               && teleported.velocity == elysia::core::Vector2{4.0f, -2.0f}
               && teleported.teleport_position == elysia::core::Vector2{120.0f, 80.0f},
           "teleport preserves velocity and forwards its destination");
}

void test_round_controller()
{
    using game::projectile::ProjectileEndReason;
    using game::session::ProjectileCompletionAction;
    using game::session::RoundController;

    RoundController round;
    round.configure(3);
    expect(round.is_aiming(), "a round starts in aiming state");
    expect(round.begin_projectile_flight() && round.is_in_flight(),
           "launch transitions to flight");
    expect(round.finish_projectile(ProjectileEndReason::Hit)
               == ProjectileCompletionAction::BeginResolution
               && round.is_resolving(),
           "a hit transitions to resolution");
    expect(round.finish_resolution() == ProjectileCompletionAction::ReturnToAiming
               && round.is_aiming() && round.completed_rounds() == 1,
           "resolution returns to aiming");

    expect(round.begin_projectile_flight(), "a new shot can begin after resolution");
    expect(round.finish_projectile(ProjectileEndReason::Expired)
               == ProjectileCompletionAction::ReturnToAiming
               && round.is_aiming() && round.completed_rounds() == 2,
           "expiration returns directly to aiming");
    expect(round.begin_projectile_flight(), "a shot can begin after expiration");
    expect(round.finish_projectile(ProjectileEndReason::OutOfBounds)
               == ProjectileCompletionAction::BeginFlagshipWarning
               && round.is_flagship_warning() && round.completed_rounds() == 3,
           "the final out-of-bounds shot starts the flagship warning");
    expect(round.begin_flagship_firing() && round.is_flagship_firing(),
           "warning advances to flagship firing");
    expect(round.finish_flagship_firing() && round.is_defeated(),
           "flagship firing advances to defeat");

    round.reset();
    expect(round.begin_projectile_flight(), "a victory shot can begin");
    round.record_impact(true);
    expect(round.is_victorious(), "defeating the objective transitions to victory");
    expect(round.finish_projectile(ProjectileEndReason::Hit)
               == ProjectileCompletionAction::None
               && round.is_victorious(),
           "projectile completion does not overwrite victory");
    round.reset();
    expect(round.is_aiming(), "reset returns victory to aiming");
}

void test_anomaly_impacts()
{
    game::anomaly::BlackHoleAnomaly black_hole({
        .center = {10.0f, 20.0f}, .event_horizon_radius = 70.0f});
    const auto consumed = black_hole.resolve_projectile_impact({});
    expect(consumed.disposition == game::projectile::ProjectileDisposition::Destroy,
           "a black-hole event horizon destroys projectiles");

    game::anomaly::WormholePortal portal({
        .center = {10.0f, 20.0f},
        .destination_center = {200.0f, 300.0f},
        .portal_radius = 75.0f,
        .exit_offset = 96.0f});
    const auto transfer = portal.resolve_projectile_impact({
        .projectile_velocity = {0.0f, -500.0f}});
    expect(transfer.disposition == game::projectile::ProjectileDisposition::Teleport,
           "wormholes request projectile teleportation");
    expect(transfer.teleport_position.has_value(), "wormholes provide an exit position");
    if (transfer.teleport_position)
    {
        expect_near(transfer.teleport_position->x, 200.0f,
                    "wormhole exit keeps the perpendicular coordinate");
        expect_near(transfer.teleport_position->y, 204.0f,
                    "wormhole exit offsets along projectile travel direction");
    }
}

void test_tenth_shot_rules()
{
    using game::projectile::ProjectileEndReason;
    using game::session::ProjectileCompletionAction;

    game::session::RoundController exhausted;
    exhausted.configure(10);
    for (int shot = 1; shot < 10; ++shot)
    {
        expect(exhausted.begin_projectile_flight(), "each pre-limit shot can launch");
        expect(exhausted.finish_projectile(ProjectileEndReason::Expired)
                   == ProjectileCompletionAction::ReturnToAiming,
               "the first nine completed shots return to aiming");
    }
    expect(exhausted.remaining_rounds() == 1, "one full shot remains before the limit");
    expect(exhausted.begin_projectile_flight(), "the tenth shot can launch normally");
    expect(exhausted.finish_projectile(ProjectileEndReason::OutOfBounds)
               == ProjectileCompletionAction::BeginFlagshipWarning
               && exhausted.is_flagship_warning(),
           "a non-winning tenth shot starts the flagship weapon");

    game::session::RoundController victorious;
    victorious.configure(10);
    for (int shot = 1; shot < 10; ++shot)
    {
        (void)victorious.begin_projectile_flight();
        (void)victorious.finish_projectile(ProjectileEndReason::Expired);
    }
    expect(victorious.begin_projectile_flight(), "a winning tenth shot can launch");
    victorious.record_impact(true);
    expect(victorious.finish_projectile(ProjectileEndReason::Hit)
               == ProjectileCompletionAction::None
               && victorious.is_victorious(),
           "tenth-shot victory takes precedence over flagship firing");
}
}

int main()
{
    test_damage();
    test_fleet_shield();
    test_projectile_motion();
    test_round_controller();
    test_anomaly_impacts();
    test_tenth_shot_rules();
    if (failures == 0)
    {
        std::cout << "All game tests passed.\n";
        return 0;
    }
    std::cerr << failures << " game test(s) failed.\n";
    return 1;
}
