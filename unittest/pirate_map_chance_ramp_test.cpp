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
};
