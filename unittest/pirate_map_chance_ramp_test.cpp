#include "external/boost/ut.hpp"
#include "battle.h"
#include "dungeon.h"
#include "testhelper.hpp"
#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "aregion.h"
#include "items.h"

namespace ut = boost::ut;

// Sums an item across every unit belonging to `f` in the region.
static int count_faction_item(ARegion *r, Faction *f, int item)
{
    int total = 0;
    for (auto obj : r->objects)
        for (auto u : obj->units)
            if (u->faction == f) total += u->items.GetNum(item);
    return total;
}

// Counts monsters of the given race still standing, ignoring the player's own units.
static int count_surviving_race(ARegion *r, Faction *player, int race)
{
    int total = 0;
    for (auto obj : r->objects)
        for (auto u : obj->units)
            if (u->faction != player) total += u->items.GetNum(race);
    return total;
}

// Pushes the map-chance ramp so far past 100 that every vessel roll succeeds and
// every success yields a TMAP. Lets a test count how many rolls were made without
// seeding a specific RNG draw.
static void force_guaranteed_tmap(UnitTestHelper &helper)
{
    json data;
    data["map_chance_ramp_turns"] = 1;
    data["map_chance_ramp_bonus"] = 100.0;
    data["tmap_share_early"] = 100;
    data["tmap_share_late"] = 100;
    helper.set_ruleset_specific_data(data);
    helper.game_object().year = 1;
    helper.game_object().month = 0;  // TurnNumber() == 1, rampTurns == 1 -> full ramp at once
    helper.game_object().UpdateMapChanceRamp();
}

// An attacker strong enough to wipe several pirate fleets in one battle.
// `observation` above the pirates' stealth of 1 makes Faction::CanSee() return 2,
// which is what CanAttack() - and therefore the old GetSides() path - demanded.
static Unit *create_pirate_hunter(UnitTestHelper &helper, ARegion *r, int observation = 0)
{
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);
    // Armed well past what the fight needs: a squadron of several hulls otherwise
    // routs while its FLAG_BEHIND officers are still standing, and a test that
    // counts loot from dead captains would be measuring the rout, not the loot.
    u->items.SetNum(I_MSWORD, 500);
    helper.set_skill_level(u, S_COMBAT, 5);
    if (observation > 0) helper.set_skill_level(u, S_OBSERVATION, observation);
    return u;
}

// Adds a fleet crewed by `crew` pirates; returns the crew unit (its object is the fleet).
static Unit *add_plain_fleet(UnitTestHelper &helper, ARegion *r, int crew)
{
    return helper.create_npc_pirate_fleet(r, crew);
}

// Adds a fleet crewed by `crew` pirates and led by a captain.
static Unit *add_elite_fleet(UnitTestHelper &helper, ARegion *r, int crew)
{
    Unit *pirates = helper.create_npc_pirate_fleet(r, crew);
    helper.create_npc_pirate_captain(r, pirates->object);
    return pirates;
}

ut::suite<"PirateMapChanceRamp"> pirate_map_chance_ramp_suite = [] {
    using namespace ut;

    // --- compute_map_chance_ramp ---

    "ramp disabled (rampTurns=0) returns unchanged defaults"_test = [] {
        MapChanceRamp r = compute_map_chance_ramp(1, /*rampTurns=*/0, /*rampBonus=*/0.5, /*shareEarly=*/10, /*shareLate=*/25);
        expect(r.multiplier == 1.0_d);
        expect(r.tmapShare == 10_i);
    };

    "turn 1 of a configured ramp is at the start of the curve"_test = [] {
        MapChanceRamp r = compute_map_chance_ramp(1, /*rampTurns=*/60, /*rampBonus=*/0.5, /*shareEarly=*/10, /*shareLate=*/25);
        expect(r.multiplier > 1.0_d && r.multiplier < 1.02_d);
        expect(r.tmapShare == 10_i);
    };

    "turn at rampTurns reaches full plateau bonus"_test = [] {
        MapChanceRamp r = compute_map_chance_ramp(60, 60, 0.5, 10, 25);
        expect(r.multiplier == 1.5_d);
        expect(r.tmapShare == 25_i);
    };

    "turn past rampTurns clamps at plateau, does not keep growing"_test = [] {
        MapChanceRamp r = compute_map_chance_ramp(9999, 60, 0.5, 10, 25);
        expect(r.multiplier == 1.5_d);
        expect(r.tmapShare == 25_i);
    };

    "negative rampTurns is treated as disabled (no divide-by-zero)"_test = [] {
        MapChanceRamp r = compute_map_chance_ramp(100, -5, 0.5, 10, 25);
        expect(r.multiplier == 1.0_d);
        expect(r.tmapShare == 10_i);
    };

    // --- apply_hideout_supply_throttle ---

    "throttle disabled (softCap=0) passes share through unchanged"_test = [] {
        expect(apply_hideout_supply_throttle(20, /*activeHideouts=*/10, /*softCap=*/0, /*floorShare=*/10) == 20_i);
    };

    "zero active hideouts leaves share unchanged"_test = [] {
        expect(apply_hideout_supply_throttle(20, 0, 8, 10) == 20_i);
    };

    "partial saturation reduces share but stays above the floor"_test = [] {
        // factor = 1 - 2/8 = 0.75; 20*0.75 = 15, which is still above the floor of 10
        expect(apply_hideout_supply_throttle(20, 2, 8, 10) == 15_i);
    };

    "full (or over) saturation clamps down to the floor, never to zero"_test = [] {
        expect(apply_hideout_supply_throttle(20, 8, 8, 10) == 10_i);
        expect(apply_hideout_supply_throttle(20, 20, 8, 10) == 10_i);  // over capacity must clamp at the floor, not go below it
    };

    // --- dungeon_total_cells ---

    "dungeon_total_cells computes whole 8x8 cells from level dimensions"_test = [] {
        expect(dungeon_total_cells(64, 64) == 64_i);  // 8x8 grid of cells
        expect(dungeon_total_cells(16, 16) == 4_i);   // 2x2 grid of cells
        expect(dungeon_total_cells(1, 2) == 0_i);     // smaller than one cell -> 0, no crash
    };

    // --- Battle fields ---

    "a freshly constructed Battle defaults to unramped values"_test = [] {
        Battle b;
        expect(b.mapChanceMultiplier == 1.0_d);
        expect(b.tmapShare == 10_i);
    };

    // --- Game::UpdateMapChanceRamp (per-turn cache) ---

    "UpdateMapChanceRamp leaves cached fields at defaults when rulesetSpecificData has no ramp keys"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        // Explicitly empty, independent of whatever ModifyTablesPerRuleset() sets by
        // default on this branch once Task 6 lands.
        helper.set_ruleset_specific_data(json::object());
        helper.game_object().UpdateMapChanceRamp();

        expect(helper.game_object().cachedMapChanceMultiplier == 1.0_d);
        expect(helper.game_object().cachedTmapShare == 10_i);
    };

    "UpdateMapChanceRamp scales the cached multiplier and share with the current turn"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json data;
        data["map_chance_ramp_turns"] = 60;
        data["map_chance_ramp_bonus"] = 0.5;
        data["tmap_share_early"] = 10;
        data["tmap_share_late"] = 20;
        helper.set_ruleset_specific_data(data);

        helper.game_object().year = 6;
        helper.game_object().month = 0;  // TurnNumber() == (6-1)*12+0+1 == 61, past the 60-turn plateau
        helper.game_object().UpdateMapChanceRamp();

        expect(helper.game_object().cachedMapChanceMultiplier == 1.5_d);
        expect(helper.game_object().cachedTmapShare == 20_i);  // no hideout_soft_cap_percent set -> unthrottled
    };

    "UpdateMapChanceRamp derives a larger hideout cap on a larger dungeon-level grid"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->level->levelType = ARegionArray::LEVEL_DUNGEON;  // same technique as dungeon_rmap_drop_test.cpp
        r->level->x = 64;
        r->level->y = 64;  // total_cells = (64/8)*(64/8) = 64 -> soft cap = max(1, 64*7/100) = 4

        json data;
        data["map_chance_ramp_turns"] = 60;
        data["map_chance_ramp_bonus"] = 0.5;
        data["tmap_share_early"] = 10;
        data["tmap_share_late"] = 20;
        data["hideout_soft_cap_percent"] = 7;
        helper.set_ruleset_specific_data(data);

        helper.game_object().year = 6;
        helper.game_object().month = 0;  // TurnNumber() == 61, plateau -> scheduled tmapShare == 20

        DungeonInstance d;
        d.type = DungeonType::DUNGEON_PIRATE_HIDEOUT;
        d.state = DungeonSlotState::ACTIVE;
        helper.game_object().activeDungeons.push_back(d);  // 1 of 4 soft-cap slots occupied

        helper.game_object().UpdateMapChanceRamp();

        // factor = 1 - 1/4 = 0.75; 20*0.75 = 15, still above the floor of 10
        expect(helper.game_object().cachedTmapShare == 15_i);
    };

    "UpdateMapChanceRamp derives a smaller hideout cap on a smaller dungeon-level grid"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        r->level->levelType = ARegionArray::LEVEL_DUNGEON;
        r->level->x = 16;
        r->level->y = 16;  // total_cells = (16/8)*(16/8) = 4 -> soft cap = max(1, 4*7/100) = 1

        json data;
        data["map_chance_ramp_turns"] = 60;
        data["map_chance_ramp_bonus"] = 0.5;
        data["tmap_share_early"] = 10;
        data["tmap_share_late"] = 20;
        data["hideout_soft_cap_percent"] = 7;
        helper.set_ruleset_specific_data(data);

        helper.game_object().year = 6;
        helper.game_object().month = 0;  // TurnNumber() == 61, plateau -> scheduled tmapShare == 20

        DungeonInstance d;
        d.type = DungeonType::DUNGEON_PIRATE_HIDEOUT;
        d.state = DungeonSlotState::ACTIVE;
        helper.game_object().activeDungeons.push_back(d);  // fills the entire soft cap of 1

        helper.game_object().UpdateMapChanceRamp();

        // The same single active hideout that only cost 5 points on the 64x64 map
        // (test above) fully saturates a 16x16 map's cap of 1 -> floored at 10.
        expect(helper.game_object().cachedTmapShare == 10_i);
    };

    // --- Game::RunBattle wiring ---

    "RunBattle copies Game's cached ramp values onto Battle outside dungeons"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json data;
        data["map_chance_ramp_turns"] = 60;
        data["map_chance_ramp_bonus"] = 0.5;
        data["tmap_share_early"] = 10;
        data["tmap_share_late"] = 20;
        helper.set_ruleset_specific_data(data);
        helper.game_object().year = 6;
        helper.game_object().month = 0;  // TurnNumber() == 61, plateau
        helper.game_object().UpdateMapChanceRamp();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 1);
        Faction *player = helper.create_faction("Player");
        Unit *attacker = helper.create_unit(player, r);
        attacker->items.SetNum(I_LEADERS, 500);

        helper.run_battle(r, attacker, pirates);

        Battle *b = helper.game_object().battles.back();
        expect(b->mapChanceMultiplier == 1.5_d);
        expect(b->tmapShare == 20_i);
    };

    "RunBattle leaves Battle at defaults when the cache itself is at defaults"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        helper.set_ruleset_specific_data(json::object());
        helper.game_object().UpdateMapChanceRamp();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 1);
        Faction *player = helper.create_faction("Player");
        Unit *attacker = helper.create_unit(player, r);
        attacker->items.SetNum(I_LEADERS, 500);

        helper.run_battle(r, attacker, pirates);

        Battle *b = helper.game_object().battles.back();
        expect(b->mapChanceMultiplier == 1.0_d);
        expect(b->tmapShare == 10_i);
    };

    "RunBattle never applies the cache inside a dungeon level"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        json data;
        data["map_chance_ramp_turns"] = 60;
        data["map_chance_ramp_bonus"] = 0.5;
        data["tmap_share_early"] = 10;
        data["tmap_share_late"] = 20;
        helper.set_ruleset_specific_data(data);
        helper.game_object().year = 6;
        helper.game_object().month = 0;
        helper.game_object().UpdateMapChanceRamp();  // cache now holds ramped, non-default values

        ARegion *r = helper.get_region(0, 2, 0);
        r->level->levelType = ARegionArray::LEVEL_DUNGEON;  // fight happens inside a hideout's own rooms
        Unit *pirates = helper.create_npc_pirate_fleet(r, 1);
        Faction *player = helper.create_faction("Player");
        Unit *attacker = helper.create_unit(player, r);
        attacker->items.SetNum(I_LEADERS, 500);

        helper.run_battle(r, attacker, pirates);

        Battle *b = helper.game_object().battles.back();
        expect(b->mapChanceMultiplier == 1.0_d);
        expect(b->tmapShare == 10_i);
    };

    // --- Army::Lose wiring ---

    "Army::Lose honors an extreme ramp: guaranteed map, guaranteed TMAP"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        // Extreme but valid config: at turn 1 with rampTurns=1, ramp=1.0 immediately.
        // rampBonus=100 pushes the scaled chance far past 100 (always succeeds regardless
        // of the RNG draw); shareLate=100 forces every successful roll to be a TMAP.
        // This proves Army::Lose is reading b->mapChanceMultiplier/tmapShare rather than
        // the old hardcoded constants, without needing to know/seed a specific RNG draw.
        json data;
        data["map_chance_ramp_turns"] = 1;
        data["map_chance_ramp_bonus"] = 100.0;
        data["tmap_share_early"] = 100;
        data["tmap_share_late"] = 100;
        helper.set_ruleset_specific_data(data);
        helper.game_object().year = 1;
        helper.game_object().month = 0;  // TurnNumber() == 1
        helper.game_object().UpdateMapChanceRamp();  // refresh the per-turn cache with the new config

        ARegion *r = helper.get_region(0, 2, 0);
        // Only rank-and-file pirates (base chance 10%) - without the ramp this would
        // usually NOT drop a map at all; with the ramp it must always drop a TMAP.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 1);
        Faction *player = helper.create_faction("Player");
        Unit *attacker = helper.create_unit(player, r);
        attacker->items.SetNum(I_LEADERS, 500);

        int result = helper.run_battle(r, attacker, pirates);
        expect(result == BATTLE_WON);

        int total_tmap = 0, total_rmap = 0;
        for (auto obj : r->objects)
            for (auto u : obj->units)
                if (u->faction == attacker->faction) {
                    total_tmap += u->items.GetNum(I_TREASURE_MAP);
                    total_rmap += u->items.GetNum(I_RESOURCE_MAP);
                }

        expect(total_tmap == 1_i) << "extreme ramp must guarantee exactly one TMAP";
        expect(total_rmap == 0_i) << "shareLate=100 must never produce an RMAP";
    };

    // -----------------------------------------------------------------------
    // Map loot is rolled per pirate vessel, not per battle.
    //
    // Clearing several fleets in one fight must pay exactly what clearing them
    // in separate fights would: the player who concentrates takes on more risk,
    // not less reward.
    //
    // Every test below runs under force_guaranteed_tmap(), so the number of
    // TMAPs recovered equals the number of rolls Army::Lose() actually made.
    // -----------------------------------------------------------------------

    "wandering monsters in a region all join one another's battle"_test = [] {
        // Control for the rule the fleet cases are measured against. Loose monsters
        // sit in the region's dummy object, so GetSides()'s `o == tar->object`
        // clause matches for every one of them and they join unconditionally -
        // no visibility check involved. Ships are what break the pattern: each
        // hull is an object of its own.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = helper.create_pirate_unit(r, 5);
        helper.create_pirate_unit(r, 5);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, first) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "a loose monster stack sat out a battle in its own region";
    };

    "two pirate fleets in one region are drawn into a single battle"_test = [] {
        // The premise the map tests rest on, and the red test for the GetSides()
        // squadron rule: the attacker here cannot identify the pirates' faction
        // (observation 0 vs stealth 1), so before that rule the second hull stayed
        // out and only the targeted ship went down.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = add_elite_fleet(helper, r, 1);
        add_elite_fleet(helper, r, 1);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        int result = helper.run_battle(r, attacker, first);
        expect(result == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "a captain survived: that fleet never joined the battle";
        expect(count_faction_item(r, player, I_COMPASS) == 2_i)
            << "one compass per dead captain";
    };

    "an attacker who can identify the pirates pulls both fleets in as well"_test = [] {
        // The other half of the truth table: with observation 2 against stealth 1
        // CanAttack() already held, so this case joined even before the squadron
        // rule. It must keep working afterwards - the rule adds a path, it does
        // not replace the old one.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = add_elite_fleet(helper, r, 1);
        add_elite_fleet(helper, r, 1);

        Unit *attacker = create_pirate_hunter(helper, r, /*observation=*/2);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, first) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "an observant attacker must still engage the whole squadron";
        expect(count_faction_item(r, player, I_COMPASS) == 2_i)
            << "one compass per dead captain";
    };

    "each elite fleet in a battle rolls for a map of its own"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = add_elite_fleet(helper, r, 1);
        add_elite_fleet(helper, r, 1);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, first) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "both hulls must actually go down before their loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 2_i)
            << "two vessels sunk must mean two rolls, not one capped roll";
    };

    "the rank-and-file crew bonus is counted per vessel, not once per battle"_test = [] {
        // Two fleets with no officers aboard: the +10 crew bonus has to apply to
        // each hull separately, otherwise the second ship is worth nothing.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = add_plain_fleet(helper, r, 1);
        add_plain_fleet(helper, r, 1);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, first) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "both hulls must actually go down before their loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 2_i)
            << "each hull carries its own crew bonus";
    };

    "a summoned swarm pays per vessel"_test = [] {
        // The CALL PIRATES scenario: a caster drags every nearby fleet into one
        // fight. Four hulls sunk must pay what four separate fights would.
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *first = add_elite_fleet(helper, r, 1);
        add_elite_fleet(helper, r, 1);
        add_plain_fleet(helper, r, 1);
        add_plain_fleet(helper, r, 1);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, first) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "the squadron routed with officers still standing: loot count is meaningless";
        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the squadron routed with crew still standing: loot count is meaningless";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 4_i)
            << "four vessels sunk must mean four rolls";
        expect(count_faction_item(r, player, I_COMPASS) == 2_i)
            << "compasses already accrue per captain and must stay that way";
    };
};
