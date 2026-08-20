#include "external/boost/ut.hpp"
#include "battle.h"
#include "testhelper.hpp"
#include "game.h"
#include "gamedata.h"
#include "object.h"
#include "aregion.h"
#include "items.h"

namespace ut = boost::ut;

// Cover for the seize -> battle -> spoils chain and for the maturity gate that
// governs it. Pirate special loot (compass, whistle, map roll) is paid only by
// units at free == 0, the same ladder GetMonSpoils uses for ordinary loot; a hull
// crewed entirely by younger pirates never even registers as a vessel.
//
// That gate is what keeps PirateSeizeEmptyShips() from minting map rolls: a prize
// crew is created at free = MONSTER_SPOILS_RECOVERY and needs that many turns
// before it is worth sinking. The two halves are tested separately elsewhere
// (pirate_seize_test.cpp, pirate_map_chance_ramp_test.cpp).
//
// NOTE: the test ruleset sets MONSTER_SPOILS_RECOVERY = 0 (unittest/rules.cpp),
// i.e. every monster it spawns is already grown. So "green" is written here as the
// literal GREEN below rather than as Globals->MONSTER_SPOILS_RECOVERY, and the tests
// that need a green prize crew age it by hand after the seizure. What ties the two
// together - that a prize is crewed at whatever the ruleset calls fresh - is asserted
// separately, and holds under either ruleset.

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

// The crew unit of a fleet object, or nullptr if the hull is empty.
static Unit *crew_of(Object *fleet)
{
    for (auto u : fleet->units)
        if (u->items.GetNum(I_PIRATES) > 0) return u;
    return nullptr;
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
    // Seize, then sink on the same turn: PirateSeizeEmptyShips() crews the prize
    // at free = MONSTER_SPOILS_RECOVERY, so the new hull pays nothing yet and the
    // squadron is worth exactly what the one grown fleet was worth.
    //
    // This is the regression for the hull-count faucet: without the maturity gate
    // the count below is 2, and every empty ship a player leaves ashore would add
    // another roll.
    // -----------------------------------------------------------------------
    "a hull seized this turn pays nothing yet"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;  // seizure only happens off the water

        // 24 crew: split is max(2, 10-15% of 24) = 2-3, topped up to the minimum
        // boarding crew of 4.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 24);
        pirates->free = 0;
        Object *prize = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        Unit *prize_crew = crew_of(prize);
        expect(prize_crew != nullptr) << "the empty ship must have been boarded";
        expect(prize_crew->items.GetNum(I_PIRATES) == 4_i)
            << "boarding crew is the 4-man minimum at this donor size";
        expect(prize_crew->free == Globals->MONSTER_SPOILS_RECOVERY)
            << "a prize crew must start at whatever the ruleset calls freshly spawned";
        expect(count_pirate_hulls(r) == 2_i)
            << "one fleet plus one prize must read as two hulls";

        // Under the shipped ruleset that assignment already means green; under the
        // test ruleset it does not, so state the intended condition explicitly.
        prize_crew->free = GREEN;

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "both hulls must go down: the prize is part of the squadron";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "only the grown fleet rolls; the prize crew is still green";
    };

    // -----------------------------------------------------------------------
    // ... and once that prize crew has come of age it is a vessel like any other.
    // -----------------------------------------------------------------------
    "a prize crew that has come of age rolls like any other hull"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;

        Unit *pirates = helper.create_npc_pirate_fleet(r, 24);
        pirates->free = 0;
        Object *prize = helper.create_empty_fleet(r, I_COG);

        helper.run_pirate_seize_empty_ships();

        Unit *prize_crew = crew_of(prize);
        expect(prize_crew != nullptr) << "the empty ship must have been boarded";
        prize_crew->free = 0;  // three turns of PostTurn later

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the whole squadron must go down before loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 2_i)
            << "a grown prize hull rolls separately from the fleet that took it";
    };

    // -----------------------------------------------------------------------
    // Scale check on the faucet: three empty ships taken in one turn still pay
    // one roll, not four. Sink the same squadron three turns later and it pays
    // four - which is correct, because by then the prizes are real pirate ships.
    // -----------------------------------------------------------------------
    "three ships seized in one turn still pay a single roll"_test = [] {
        UnitTestHelper helper;
        helper.initialize_game();
        helper.setup_turn();
        force_guaranteed_tmap(helper);

        ARegion *r = helper.get_region(0, 2, 0);
        r->type = R_PLAIN;

        // 30 crew survives three seizures and stays above the donor floor of 20:
        // 30 -> 26-27 -> 22-25 -> 19-23, each prize crewed by the 4-man minimum.
        Unit *pirates = helper.create_npc_pirate_fleet(r, 30);
        pirates->free = 0;
        helper.create_empty_fleet(r, I_COG, "Ship A");
        helper.create_empty_fleet(r, I_COG, "Ship B");
        helper.create_empty_fleet(r, I_COG, "Ship C");

        helper.run_pirate_seize_empty_ships();

        expect(count_pirate_hulls(r) == 4_i)
            << "three prizes plus the fleet that took them";

        // Every boarding crew is fresh off the donor; see the note at the top of the
        // file for why the test ruleset needs this spelled out.
        for (auto obj : r->objects) {
            Unit *crew = crew_of(obj);
            if (crew && crew != pirates) crew->free = GREEN;
        }

        Unit *attacker = create_pirate_hunter(helper, r);
        Faction *player = attacker->faction;

        expect(helper.run_battle(r, attacker, pirates) == BATTLE_WON);

        expect(count_surviving_race(r, player, I_PIRATES) == 0_i)
            << "the whole squadron must go down before loot is counted";
        expect(count_faction_item(r, player, I_TREASURE_MAP) == 1_i)
            << "empty ships left ashore must not multiply the map faucet";
    };
};
