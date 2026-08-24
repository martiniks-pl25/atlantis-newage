#include "external/boost/ut.hpp"
#include "battle.h"
#include "testhelper.hpp"
#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "aregion.h"
#include "items.h"

namespace ut = boost::ut;

// Cover for the seize -> battle -> spoils chain and for the structural faucet
// closure that the merge buys. Pirate special loot (compass, whistle, map roll)
// is paid per vessel: Army::Lose() rolls once per fleet OBJECT (vessel_for on
// the object), so a pirate fleet that has absorbed several abandoned ships is
// still exactly one hull and still yields exactly one map roll. The merge never
// mints a second object, which is what keeps PirateSeizeEmptyShips() from
// multiplying the map faucet (design 8.4).
//
// The general maturity gate - a unit's `free` field - is asserted on the
// fleet's own crew below and is unchanged by the merge; the ramp itself is
// tested in pirate_map_chance_ramp_test.cpp.

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

// Counts fleet objects in the region that carry at least one pirate.
static int count_pirate_hulls(ARegion *r)
{
    int hulls = 0;
    for (auto obj : r->objects) {
        if (!obj->IsFleet()) continue;
        for (auto u : obj->units) {
            if (u->items.GetNum(I_PIRATES) > 0) { hulls++; break; }
        }
    }
    return hulls;
}

// Pushes the map-chance ramp so far past 100 that every vessel roll succeeds and
// every success yields a TMAP, so the number of maps recovered *is* the number of
// rolls Army::Lose() made. Same device as pirate_map_chance_ramp_test.cpp.
static void force_guaranteed_tmap(UnitTestHelper &helper)
{
    json data;
    data["map_chance_ramp_turns"] = 1;
    data["map_chance_ramp_bonus"] = 100.0;
    data["tmap_share_early"] = 100;
    data["tmap_share_late"] = 100;
    helper.set_ruleset_specific_data(data);
    helper.game_object().year = 1;
    helper.game_object().month = 0;
    helper.game_object().UpdateMapChanceRamp();
}

// An attacker armed far past what the fight needs: a squadron that routs leaves
// survivors, and survivors pay no spoils, so an under-armed hunter would make the
// test measure the rout instead of the loot. Observation stays 0 on purpose - the
// squadron rule in GetSides() has to pull every hull in without the attacker
// identifying the pirates' faction.
static Unit *create_pirate_hunter(UnitTestHelper &helper, ARegion *r)
{
    Faction *player = helper.create_faction("Player");
    Unit *u = helper.create_unit(player, r);
    u->items.SetNum(I_LEADERS, 500);
    u->items.SetNum(I_MSWORD, 500);
    helper.set_skill_level(u, S_COMBAT, 5);
    return u;
}

// A maturity level that is not "grown", independent of what the ruleset uses.
static const int GREEN = 3;

ut::suite<"PirateSeizeSpoils"> pirate_seize_spoils_suite = [] {
    using namespace ut;

    // -----------------------------------------------------------------------
    // A plain fleet - no captain, no bosun - is worth a map roll of its own.
    // That is the flat +10 crew bonus in Army::Lose(): a vessel earns it because
    // grown pirates died aboard it, not because an officer did. Officers only add
    // +20 each on top.
    // -----------------------------------------------------------------------
    "a grown crew with no officers aboard still rolls for a map"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = 0;  // Elite: full spoils

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the hull must actually go down before its loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "rank-and-file pirates carry a map chance of their own";
        expect(count_faction_item(r, player, I_COMPASS) == 0_i)
            << "a compass comes from a captain, and there was none";
        expect(count_faction_item(r, player, I_BOSUN_WHISTLE) == 0_i)
            << "a whistle comes from a bosun, and there was none";
    };

    // -----------------------------------------------------------------------
    // The maturity gate, on the crew: a hull whose report line reads "No spoils."
    // really has none, maps included.
    // -----------------------------------------------------------------------
    "a crew that has not come of age carries no map"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = GREEN;  // Novice, just spawned

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the hull must go down: this test is about loot, not about routing";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 0_i)
            << "a green crew must not pay a map even under a guaranteed ramp";
    };

    // -----------------------------------------------------------------------
    // The same gate on officers: rank rides on free, and so does the drop.
    // -----------------------------------------------------------------------
    "a corsair carries no compass until he has come of age"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        Unit *captain = helper.create_npc_pirate_captain(r, pirates->object);
        pirates->free = GREEN;
        captain->free = GREEN;

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATE_CAPTAIN) == 0_i)
            << "the captain must go down before his loot is counted";
        expect(count_faction_item(r, player, I_COMPASS) == 0_i)
            << "a corsair has not yet earned a compass";
        expect(count_faction_item(r, player, I_BOSUN_WHISTLE) == 0_i)
            << "and no whistle either: there is no bosun aboard";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 0_i)
            << "an all-green hull registers no vessel and rolls nothing";
    };

    // -----------------------------------------------------------------------
    // Design 8.4: a fleet that absorbs three ships in one turn is still ONE
    // fleet object, so one battle yields exactly one map roll under a guaranteed
    // ramp. Before the merge each seizure minted its own prize object and the
    // count below would be 4 with four rolls on the faucet.
    // -----------------------------------------------------------------------
    "a fleet that seizes three ships in one turn stays one hull and pays one map roll"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        // Keep the crew "crowded" at 1% and raise the per-turn cap so a single
        // turn absorbs all three hulls; the ramp keys above are untouched.
        helper.game_object().rulesetSpecificData["pirate_seize_fill_pct"] = 1;
        helper.game_object().rulesetSpecificData["pirate_seize_max_per_turn"] = 3;

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;  // seizure only happens off the water

        Unit *pirates = helper.create_npc_pirate_fleet(r, 40);
        pirates->free = 0;
        helper.create_empty_fleet(r, I_COG, "Ship A");
        helper.create_empty_fleet(r, I_COG, "Ship B");
        helper.create_empty_fleet(r, I_COG, "Ship C");

        helper.run_pirate_seize_empty_ships();

        expect(count_pirate_hulls(r) == 1_i)
            << "three merged prizes plus the fleet must read as one hull";
        expect(pirates->object->GetNumShips(I_COG) == 4_i)
            << "the fleet must have absorbed all three hulls";

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the hull must go down before its loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "one fleet object means one map roll, however many ships it absorbed";
    };
};

// The map-roll chance formula, extracted as a pure helper so the role rule is
// testable without a battle: each officer role pays its vessel entry once.
// Captain, bosun and admiral contribute +20 each at most once however many of
// that role died there; the crew contributes +10 once. This applies to a fleet
// hull and a dungeon room alike.
ut::suite<"PirateVesselMapChance"> pirate_vessel_map_chance_suite = [] {
    using namespace ut;

    "each officer role pays once per vessel entry"_test = [] {
        // Born elite: captain + bosun + crew - the common case, unchanged.
        expect(pirate_vessel_map_chance(true, true, false, true) == 50_i)
            << "captain 20 + bosun 20 + crew 10";
        // A hull where several bosuns died: the bosun role still pays once.
        expect(pirate_vessel_map_chance(false, true, false, true) == 30_i)
            << "two bosuns give the same chance as one bosun";
        expect(pirate_vessel_map_chance(false, true, false, true) < 50_i)
            << "strictly less than a per-officer sum (two bosuns + crew would be 50)";
        // A dungeon room: several bosuns and several captains, plus the Admiral.
        expect(pirate_vessel_map_chance(true, true, false, true) == 50_i)
            << "several bosuns and several captains pay 20 + 20 + 10";
        expect(pirate_vessel_map_chance(true, true, true, true) == 70_i)
            << "plus another 20 when the Admiral dies there";
        expect(pirate_vessel_map_chance(true, true, true, true) < 130_i)
            << "strictly less than a per-officer sum (3 bosuns + 2 captains + admiral would be 130)";
    };

    // A fleet hull with one captain and two bosuns is still one vessel entry:
    // one compass, one map roll. The two bosuns roll their whistle independently
    // but add nothing to the vessel chance beyond the single bosun role.
    "a captain and two bosuns on one hull still pay one vessel roll"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        Unit *pirates = helper.create_npc_pirate_fleet(r, 12);
        pirates->free = 0;
        Unit *captain = helper.create_npc_pirate_captain(r, pirates->object);
        captain->free = 0;
        Unit *bos1 = helper.create_npc_pirate_bosun(r, pirates->object);
        bos1->free = 0;
        Unit *bos2 = helper.create_npc_pirate_bosun(r, pirates->object);
        bos2->free = 0;

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the hull must go down before its loot is counted";
        expect(count_faction_item(r, player, I_COMPASS) == 1_i)
            << "one compass from the one captain";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "one hull means one map roll, however many officers died on it";
    };

    // A hideout boss room - crew, two captains, three bosuns, the Admiral - is
    // one vessel entry (the room's object): one crown, one compass per captain,
    // and one map roll, however many officers died in it.
    "a hideout room with several of each role pays one vessel entry"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        helper.create_monster(r, I_PIRATES, 80);
        helper.create_monster(r, I_PIRATE_CAPTAIN, 1);
        helper.create_monster(r, I_PIRATE_CAPTAIN, 1);
        helper.create_monster(r, I_PIRATE_BOSUN, 1);
        helper.create_monster(r, I_PIRATE_BOSUN, 1);
        helper.create_monster(r, I_PIRATE_BOSUN, 1);
        Unit *admiral = helper.create_monster(r, I_PIRATE_KING, 1);

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, admiral) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the room must go down before its loot is counted";
        expect(count_faction_item(r, player, I_CROWN) == 1_i)
            << "the Admiral drops one crown";
        expect(count_faction_item(r, player, I_COMPASS) == 2_i)
            << "one compass per captain";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "one room means one map roll, however many officers died in it";
    };
};
