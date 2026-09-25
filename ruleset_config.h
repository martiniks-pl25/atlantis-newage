#pragma once

#include <algorithm>
#include <array>

/**
 * @file ruleset_config.h
 * @brief Typed ruleset parameters, grouped by subsystem.
 *
 * The engine declares the structs and their defaults; the defaults describe how the
 * engine behaves with no ruleset opinion. A ruleset provides ruleset_config() and
 * overrides only what differs, by field name with C++20 designated initializers:
 *
 * @code
 *   const RulesetConfig& ruleset_config() {
 *       static const RulesetConfig config {
 *           .pirates = { .treasure_map = { .decipher_chance = 30 } },
 *       };
 *       return config;
 *   }
 * @endcode
 *
 * New tunables go here rather than into GameDefs (positional initializer), the C
 * globals in rules.cpp or Game::rulesetSpecificData (untyped string keys). Player
 * text that states a number reads it from here too, so text and mechanics cannot
 * drift apart.
 *
 * @see neworigins/ruleset_config.cpp, docs/ARCHITECTURE.md ("Ruleset config")
 */

/**
 * @brief EXPLORE TMAP: deciphering a treasure map to find a pirate hideout.
 */
struct TreasureMapRules {
    int decipher_chance    = 25;  ///< % per month of study to decipher the map
    int compass_multiplier = 2;   ///< decipher_chance is multiplied by this with a compass
    int burn_on_fail       = 50;  ///< % a failed attempt destroys the map

    /**
     * @brief Chance, in percent, that one month of EXPLORE TMAP deciphers the map.
     * @param has_compass whether the exploring unit carries a compass
     * @return decipher_chance, times compass_multiplier with a compass, capped at 100
     */
    [[nodiscard]] constexpr int chance(const bool has_compass) const noexcept {
        const int c = has_compass ? decipher_chance * compass_multiplier : decipher_chance;
        return std::clamp(c, 0, 100);
    }
};

/**
 * @brief Pirate land-crew press gangs (Game::PirateRecruitLandCrew, npc.cpp).
 *
 * A docked fleet works the hex it sits in; a fleet standing offshore works the
 * richest land neighbour at a reduced rate.
 */
struct RecruitRules {
    /// The per-turn intake ceiling is Object::GetFleetSize() + rng(intake_up) -
    /// rng(intake_down): at 4 and 2 a cog that needs six hands signs on five to nine.
    int intake_up    = 1;
    int intake_down  = 1;  ///< see intake_up
    int pop_cost     = 1;  ///< people the region loses per hand kept; 1 = what a player's recruiter costs
    int offshore_pct = 50; ///< % of the land intake an offshore fleet gets; 0 disables offshore recruitment
};

/**
 * @brief Pirates merging abandoned ships into their own fleet (Game::PirateSeizeEmptyShips, npc.cpp).
 */
struct SeizeRules {
    int min_crew      = 20;    ///< crew a fleet needs before it boards anything
    int fill_pct      = 50;    ///< "crowded" threshold, % of the hull's crew capacity
    int max_per_turn  = 1;     ///< ships merged into one pirate fleet per turn
    bool allow_slower = false; ///< also take ships slower than the fleet (e.g. a raft)
    bool offshore     = true;  ///< let a fleet in the water seize from land neighbours
};

/**
 * @brief Pirate fleet maturation: bosun, captain, and captainless-fleet rendezvous
 * (Game::PromotePirateFleets, npc.cpp; cooldown consumed in battle.cpp on a captain's death).
 */
struct PromotionRules {
    /// Thresholds are absolute crew counts, officers included, so promoting a pirate
    /// into an officer never drops a fleet back under its own bar.
    int bosun_crew   = 75;
    /// Per-fleet, per-turn % roll for a bosun; paces how fast eligible fleets are
    /// served, not how many eventually carry one.
    int bosun_chance = 30;
    int captain_crew = 120;  ///< effective crew at which a fleet with a bosun earns a captain
    /// Captain ceiling = max(1, water_hexes * this / 1000).
    int elite_captain_per_mille = 10;
    /// Share of the ceiling reserved for the surface; the deep holds the rest.
    int elite_captain_surface_share_pct = 67;
    bool merge   = true; ///< let two eligible captainless fleets merge
    int cooldown = 6;    ///< turns a fleet waits after its captain died (set in Army::Lose)
};

/**
 * @brief Pirate special-loot ramp: how often a sunk vessel drops a crew map or
 * treasure map, and the hideout-supply throttle on the treasure-map share
 * (Game::UpdateMapChanceRamp, battle.cpp).
 */
struct MapDropRules {
    /// The mature crew's per-vessel map chance and the treasure-map share both ramp
    /// as "turn N = N%", capped at this percent; 0 leaves the ramp dormant.
    int chance_cap = 0;
    int tmap_share_floor = 10;        ///< % the hideout-supply throttle never pushes the TMAP share below
    /// Active hideouts, as % of dungeon-level cells, at which the TMAP share starts
    /// to throttle down; 0 = throttle off.
    int hideout_soft_cap_percent = 0;
};

/**
 * @brief Pirate fleet generation (Game::MakePirateFleet, npc.cpp).
 */
struct SpawnRules {
    int elite_spawn_pct = 10; ///< % chance a newly spawned fleet is born elite (captain + bosun)
};

/**
 * @brief CAST CALL_PIRATES bosun's whistle wear (Game::RunCallPirates, spells.cpp).
 */
struct WhistleRules {
    int break_pct = 5; ///< % chance the whistle breaks after a summon it triggered
};

/**
 * @brief Pirate fleet wandering route length (Unit::DefaultOrders, unit.cpp).
 *
 * A weight table where index + 1 is the number of steps; an empty or zero-sum
 * table falls back to a compiled-in distribution rather than an empty route.
 */
struct RouteRules {
    std::array<int, 3> step_weights = { 30, 40, 30 }; ///< weight per step count: one, two, three steps
};

/// Pirate tunables.
struct PirateRules {
    TreasureMapRules treasure_map;
    RecruitRules recruit;
    SeizeRules seize;
    PromotionRules promotion;
    MapDropRules map_drop;
    SpawnRules spawn;
    WhistleRules whistle;
    RouteRules route;
};

/**
 * @brief Range checks for pirate values, run at compile time by each ruleset
 * (`static_assert(valid(config.pirates))`): an out-of-range value fails the build
 * rather than being clamped at its read site.
 */
[[nodiscard]] consteval bool valid(const PirateRules& p) noexcept
{
    return p.treasure_map.decipher_chance >= 0 && p.treasure_map.decipher_chance <= 100
        && p.treasure_map.compass_multiplier >= 1
        && p.treasure_map.burn_on_fail >= 0 && p.treasure_map.burn_on_fail <= 100
        && p.recruit.intake_up >= 1 && p.recruit.intake_down >= 1 && p.recruit.pop_cost >= 1
        && p.recruit.offshore_pct >= 0
        && p.seize.min_crew >= 0 && p.seize.fill_pct >= 0 && p.seize.max_per_turn >= 0
        && p.promotion.bosun_crew >= 1 && p.promotion.bosun_chance >= 0
        && p.promotion.captain_crew >= 1 && p.promotion.elite_captain_per_mille >= 0
        && p.promotion.elite_captain_surface_share_pct >= 0
        && p.promotion.elite_captain_surface_share_pct <= 100
        && p.promotion.cooldown >= 0
        && p.spawn.elite_spawn_pct >= 0 && p.spawn.elite_spawn_pct <= 100
        && p.whistle.break_pct >= 0 && p.whistle.break_pct <= 100;
}

/// All typed ruleset parameters.
struct RulesetConfig {
    PirateRules pirates;
};

/**
 * @brief The active ruleset's parameters. Defined by the ruleset, read-only.
 */
[[nodiscard]] const RulesetConfig& ruleset_config();
